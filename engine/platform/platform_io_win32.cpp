#include "platform_io.hpp"

#include <windows.h>

#include "win32_utf.hpp"

namespace platform {

// Гранулярность выделения на Windows — 64 КБ, но zero-parse раскладку бандла (#5) она не
// задевает: view один и с НУЛЕВЫМ смещением, а кратность 64 КБ ограничивает только аргумент
// смещения MapViewOfFile. База при этом выровнена на те же 64 КБ — строже, чем POSIX-страница,
// поэтому PAYLOAD_ALIGN=16 и alignof(AssetEntry) выполняются заведомо.
bool MappedFile::open(const std::string& utf8_path) {
    close();
    const std::wstring wide = win32::widen(utf8_path);
    if (wide.empty()) return false;
    // Широкий share-режим — чтобы окно между CreateFileW и CloseHandle никому не мешало: пекарь
    // в этот момент может писать соседний файл и подменять имя. Отдать бандл под подмену, ПОКА
    // жив вью, он всё равно не позволяет — это держит секция, и снимается только close()
    // (см. ограничение replace_file в platform_fs.hpp).
    HANDLE file = CreateFileW(wide.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER file_size{};
    // Пустой файл секцией не отображается (CreateFileMapping отвергает нулевой размер) —
    // отсекаем заранее, симметрично POSIX-проверке st_size <= 0.
    bool bad_size = !GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0;
    // Только для 32-битной сборки: там бандл >4 ГБ в size_t не влезает, и усечённый size_ дал бы
    // чтение мимо вью. На 64 битах сравнение тождественно ложно, потому и под if constexpr.
    if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
        if (static_cast<uint64_t>(file_size.QuadPart) > SIZE_MAX) bad_size = true;
    }
    if (bad_size) {
        CloseHandle(file);
        return false;
    }

    HANDLE section = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (section == nullptr) {
        CloseHandle(file);
        return false;
    }
    void* view = MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0); // offset 0, весь файл
    // Секция держит собственную ссылку на файл, а view — на секцию: оба хендла после
    // отображения не нужны и закрываются сразу (гейт 3 — без утечки хендлов).
    CloseHandle(section);
    CloseHandle(file);
    if (view == nullptr) return false;

    data_ = static_cast<const uint8_t*>(view);
    size_ = static_cast<size_t>(file_size.QuadPart);
    return true;
}

void MappedFile::close() {
    if (data_) UnmapViewOfFile(data_);
    data_ = nullptr;
    size_ = 0;
}

IoCaps MappedFile::caps() {
    return IoCaps{/*mmap=*/true, /*direct_stream=*/false};
}

} // namespace platform
