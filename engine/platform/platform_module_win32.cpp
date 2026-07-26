#include "platform_module.hpp"

#include <windows.h>

#include <atomic>
#include <string>

#include "platform_fs.hpp"
#include "win32_utf.hpp"

namespace platform {
namespace {

std::string temp_copy_path(const std::string& src) {
    static std::atomic<unsigned> counter{0};
    // Копия кладётся РЯДОМ с оригиналом, а не в %TEMP%: зависимости DLL ищутся в каталоге
    // загруженного модуля, и копия, уехавшая в чужой каталог, не нашла бы соседние библиотеки.
    // Уникальность — pid + тик + счётчик. Одного pid мало: Windows переиспользует их агрессивно,
    // и .tmp, оставшийся залоченным от аварийно упавшего прошлого экземпляра, заклинил бы
    // hot-reload навсегда. Тик разводит запуски, счётчик — открытия внутри одного.
    return src + "." + std::to_string(GetCurrentProcessId()) + "." +
           std::to_string(GetTickCount64()) + "." + std::to_string(counter.fetch_add(1)) + ".tmp";
}

thread_local std::string g_error;

// LOAD_WITH_ALTERED_SEARCH_PATH задокументирован как неопределённое поведение на относительном
// пути, а зовут нас именно относительными (`build/plugin_gravity.dll` из CI). Разворачиваем сами.
std::wstring absolute(const std::wstring& path) {
    const DWORD need = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (need == 0) return std::wstring();
    std::wstring full(need, L'\0');
    const DWORD got = GetFullPathNameW(path.c_str(), need, full.data(), nullptr);
    if (got == 0 || got >= need) return std::wstring();
    full.resize(got);
    return full;
}

} // namespace

bool Module::open(const std::string& utf8_path) {
    close();
    // Занятое имя — не фатальная ошибка, а исход столкновения: берём следующее. Без повтора одна
    // невезучая копия останавливала бы hot-reload до ручной уборки каталога.
    std::string copy;
    for (int attempt = 0; attempt < 8 && copy.empty(); ++attempt) {
        const std::string candidate = temp_copy_path(utf8_path);
        if (copy_file(utf8_path, candidate)) copy = candidate;
    }
    if (copy.empty()) {
        g_error = "copy for hot-reload failed: " + utf8_path;
        return false;
    }
    const std::wstring wide = win32::widen(copy);
    if (wide.empty()) {
        remove_file(copy);
        g_error = "path is not valid UTF-8: " + utf8_path;
        return false;
    }
    const std::wstring full = absolute(wide);
    if (full.empty()) {
        remove_file(copy);
        g_error = "GetFullPathNameW failed: " + utf8_path;
        return false;
    }
    // LOAD_WITH_ALTERED_SEARCH_PATH: зависимости искать от каталога модуля, а не от рабочего
    // каталога процесса — иначе плагин, лежащий не рядом с exe, не найдёт своих DLL.
    const HMODULE h = LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) {
        const DWORD code = GetLastError();
        remove_file(copy);
        g_error = "LoadLibraryExW failed (code " + std::to_string(code) + "): " + utf8_path;
        return false;
    }
    handle_ = h;
    temp_ = copy;
    return true;
}

void Module::close() {
    if (handle_) FreeLibrary(static_cast<HMODULE>(handle_));
    handle_ = nullptr;
    // Удаление после FreeLibrary: до выгрузки образ залочен. Неудача не диагностируется — файл
    // мог остаться залоченным чужой ссылкой, и это не отменяет успешной выгрузки.
    if (!temp_.empty()) {
        remove_file(temp_);
        temp_.clear();
    }
}

void* Module::symbol(const char* name) const {
    if (!handle_) return nullptr;
    void* sym = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
    // Диагностику промаха обязаны давать оба шва: hotreload-host печатает last_error() после
    // неудачного symbol(), и молчащий win32 подсунул бы туда текст от прошлого open().
    if (sym == nullptr) {
        g_error = "symbol not found (code " + std::to_string(GetLastError()) + "): " + name;
    }
    return sym;
}

const char* Module::last_error() {
    return g_error.empty() ? "unknown error" : g_error.c_str();
}

} // namespace platform
