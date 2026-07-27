#include "platform_shmem.hpp"

#include <windows.h>

#include <cstdint>
#include <string_view>

#include "win32_utf.hpp"

namespace platform {
namespace {

// `Local\` — пространство имён сеанса. Глобальное (`Global\`) требует SeCreateGlobalPrivilege и
// пробивает границу сеанса: редактор и запущенная им игра всегда в одном, шире нам не нужно.
constexpr std::wstring_view kPrefix = L"Local\\";

std::wstring decorate(const std::string& name) {
    return std::wstring(kPrefix) + win32::widen(name);
}

} // namespace

bool SharedMemory::open(const std::string& name, size_t size, bool create, bool writable) {
    if (addr_ != nullptr || size == 0 || !detail::shmem_name_ok(name)) return false;
    const std::wstring wname = decorate(name);
    // widen() вернул пусто → имя непредставимо в UTF-16; голый префикс секцией быть не должен.
    if (wname.size() <= kPrefix.size()) return false;

    HANDLE section = nullptr;
    if (create) {
        const DWORD hi = static_cast<DWORD>(static_cast<uint64_t>(size) >> 32);
        const DWORD lo = static_cast<DWORD>(static_cast<uint64_t>(size) & 0xFFFFFFFFu);
        // Секция без файла на диске — эквивалент shm_open: страницы обеспечивает pagefile.
        // PAGE_READWRITE даже при writable=false: писать будет ребёнок, владелец лишь смотрит.
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, hi, lo,
                                     wname.c_str());
        // O_EXCL-семантика: CreateFileMappingW при совпадении имени МОЛЧА отдаёт чужую секцию,
        // и владелец начал бы читать чужое зеркало вместо своего. Ошибку проверяем до nullptr:
        // ERROR_ALREADY_EXISTS приходит при УСПЕШНОМ вызове.
        if (section != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(section);
            return false;
        }
    } else {
        section = OpenFileMappingW(writable ? FILE_MAP_WRITE : FILE_MAP_READ, FALSE, wname.c_str());
    }
    if (section == nullptr) return false;

    // Размер вью задаётся явно: MapViewOfFile с dwNumberOfBytesToMap больше секции отказывает,
    // и завышенный запрос читателя виден отказом здесь, а не SIGBUS'ом на первом обращении.
    void* view = MapViewOfFile(section, writable ? FILE_MAP_WRITE : FILE_MAP_READ, 0, 0, size);
    if (view == nullptr) {
        CloseHandle(section);
        return false;
    }

    // Хендл НЕ закрывается: view продлевает жизнь памяти, но не имени объекта — с последним
    // хендлом секция уходит из каталога имён, и присоединение ребёнка по имени (решение 2)
    // отказало бы при живых страницах. Здесь Windows расходится с POSIX, где имя живёт до
    // shm_unlink; закрывается хендл только в close().
    native_ = reinterpret_cast<intptr_t>(section);
    addr_ = view;
    size_ = size;
    name_ = name;
    owner_ = create;
    writable_ = writable;
    return true;
}

void SharedMemory::close() {
    if (addr_ != nullptr) {
        UnmapViewOfFile(addr_);
        addr_ = nullptr;
    }
    if (native_ != 0) {
        CloseHandle(reinterpret_cast<HANDLE>(native_));
        native_ = 0;
    }
    size_ = 0;
    name_.clear();
    owner_ = false;
    writable_ = false;
}

void SharedMemory::unlink(const std::string&) {
    // Осиротевшего сегмента здесь не бывает: секция живёт по счётчику ссылок и исчезает, когда
    // закрылись все хендлы и вью обоих процессов. Отвязывать по имени нечего и незачем.
}

} // namespace platform
