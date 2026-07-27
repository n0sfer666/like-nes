#include "platform_fs.hpp"

#include "platform_env.hpp"
#include "platform_path.hpp"

#include <windows.h>

#include <io.h>
#include <share.h>

#include <vector>

#include "win32_utf.hpp"

namespace platform {
namespace {

DWORD attrs(const std::string& path) {
    const std::wstring wide = win32::widen(path);
    if (wide.empty()) return INVALID_FILE_ATTRIBUTES;
    return GetFileAttributesW(wide.c_str());
}

} // namespace

std::string exe_path() {
    // MAX_PATH не потолок: путь длиннее возвращается усечённым с ERROR_INSUFFICIENT_BUFFER,
    // и каталог рядом с exe искался бы не там. Растим буфер, пока не влезет.
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) return win32::narrow(buf.data(), n);
        if (buf.size() >= 32768) return {}; // предел пути NT — дальше расти незачем
        buf.resize(buf.size() * 2);
    }
}

bool file_stamp(const std::string& path, int64_t& out) {
    const std::wstring wide = win32::widen(path);
    if (wide.empty()) return false;
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &d) == 0) return false;
    const uint64_t mtime = (static_cast<uint64_t>(d.ftLastWriteTime.dwHighDateTime) << 32) |
                           d.ftLastWriteTime.dwLowDateTime;
    const uint64_t size = (static_cast<uint64_t>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
    out = static_cast<int64_t>(mtime * 1099511628211ull + size);
    return true;
}

std::FILE* open_file(const std::string& utf8_path, const char* mode) {
    const std::wstring wpath = win32::widen(utf8_path);
    const std::wstring wmode = win32::widen(mode);
    if (wpath.empty() || wmode.empty()) return nullptr;
    // _wfopen помечен небезопасным (C4996), но замена на _wfopen_s меняет ПОВЕДЕНИЕ: файлы,
    // открытые им, не разделяются (dwShareMode=0), и открытие падает с ERROR_SHARING_VIOLATION,
    // если на файл жив хоть один хендл — например, секция MappedFile этого же процесса. Выглядело
    // бы это как «бандл пропал». _wfsopen задаёт режим доступа явно и совпадает с _wfopen
    // байт-в-байт по семантике: _SH_DENYNO — то, что _wfopen ставит по умолчанию.
    return _wfsopen(wpath.c_str(), wmode.c_str(), _SH_DENYNO);
}

bool file_exists(const std::string& path) {
    const DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool is_dir(const std::string& path) {
    const DWORD a = attrs(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool make_dir(const std::string& path) {
    const std::wstring wide = win32::widen(path);
    if (wide.empty()) return false;
    if (CreateDirectoryW(wide.c_str(), nullptr) != 0) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS && is_dir(path);
}

bool remove_file(const std::string& path) {
    const std::wstring wide = win32::widen(path);
    if (wide.empty()) return false;
    if (DeleteFileW(wide.c_str()) != 0) return true;
    const DWORD err = GetLastError();
    return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
}

bool sync_file(std::FILE* f) {
    return std::fflush(f) == 0 && _commit(_fileno(f)) == 0;
}

void sync_dir_of(const std::string&) {}

bool replace_file(const std::string& from, const std::string& to) {
    const std::wstring wfrom = win32::widen(from), wto = win32::widen(to);
    if (wfrom.empty() || wto.empty()) return false;
    // WRITE_THROUGH: не возвращаться, пока перемещение не на носителе — это и есть здешний
    // эквивалент fsync по каталогу, которого на POSIX требует та же транзакция.
    return MoveFileExW(wfrom.c_str(), wto.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool copy_file(const std::string& src, const std::string& dst) {
    const std::wstring wsrc = win32::widen(src), wdst = win32::widen(dst);
    if (wsrc.empty() || wdst.empty()) return false;
    return CopyFileW(wsrc.c_str(), wdst.c_str(), /*bFailIfExists=*/FALSE) != 0;
}

// Через шов, а не через узкий CRT: тот отдаёт ANSI-строку, и %APPDATA% профиля `C:\Users\Пётр\`
// вернулся бы с подменёнными символами — сейв уехал бы мимо каталога либо не создался вовсе.
std::string user_data_dir(const std::string& app_name) {
    std::string base;
    if (!env_var("APPDATA", base)) return {};
    return is_absolute(base) ? base + "/" + app_name : std::string{};
}

} // namespace platform
