#include "platform_fs.hpp"

#include "platform_env.hpp"
#include "platform_path.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace platform {

std::string exe_path() {
#if defined(__APPLE__)
    uint32_t n = 0;
    _NSGetExecutablePath(nullptr, &n);
    std::vector<char> buf(n);
    if (_NSGetExecutablePath(buf.data(), &n) != 0) return {};
    // _NSGetExecutablePath не канонизирует (symlink/.. не раскрыты) → realpath.
    char real[PATH_MAX];
    return realpath(buf.data(), real) ? std::string(real) : std::string(buf.data());
#else
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(buf)) return {}; // усечение → ошибка
    return std::string(buf, static_cast<size_t>(n));
#endif
}

bool file_stamp(const std::string& path, int64_t& out) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) return false;
#if defined(__APPLE__)
    const int64_t ns = static_cast<int64_t>(st.st_mtimespec.tv_sec) * 1000000000 +
                       st.st_mtimespec.tv_nsec;
#else
    const int64_t ns = static_cast<int64_t>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
    // Композит mtime+size: правка видна даже при грубой (1с) mtime-гранулярности ФС, если сменился
    // размер. (Тот же размер в ту же секунду — редкий промах; nsec-mtime на APFS/ext4 его снимает.)
    const uint64_t mix = static_cast<uint64_t>(ns) * 1099511628211ull +
                         static_cast<uint64_t>(st.st_size);
    out = static_cast<int64_t>(mix);
    return true;
}

std::FILE* open_file(const std::string& utf8_path, const char* mode) {
    return std::fopen(utf8_path.c_str(), mode);
}

bool file_exists(const std::string& path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool is_dir(const std::string& path) {
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool list_dir(const std::string& dir, std::vector<std::string>& out) {
    out.clear();
    DIR* d = ::opendir(dir.c_str());
    if (!d) return false;
    while (const dirent* e = ::readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        out.push_back(name);
    }
    ::closedir(d);
    return true;
}

bool make_dir(const std::string& path) {
    // 0700, а не 0755: XDG требует приватности для $XDG_DATA_HOME, и прогресс игрока на
    // многопользовательской машине иначе читает любой локальный пользователь.
    if (::mkdir(path.c_str(), 0700) == 0) return true;
    return errno == EEXIST && is_dir(path);
}

bool remove_file(const std::string& path) {
    return ::unlink(path.c_str()) == 0 || errno == ENOENT;
}

bool sync_file(std::FILE* f) {
    return std::fflush(f) == 0 && ::fsync(fileno(f)) == 0;
}

void sync_dir_of(const std::string& file_path) {
    const size_t slash = file_path.find_last_of('/');
    const std::string dir = slash == std::string::npos ? std::string(".") : file_path.substr(0, slash);
    const int fd = ::open(dir.c_str(), O_RDONLY);
    if (fd < 0) return;
    ::fsync(fd);
    ::close(fd);
}

bool replace_file(const std::string& from, const std::string& to) {
    return std::rename(from.c_str(), to.c_str()) == 0;
}

bool copy_file(const std::string& src, const std::string& dst) {
    FILE* in = open_file(src, "rb");
    if (!in) return false;
    FILE* out = open_file(dst, "wb");
    if (!out) {
        std::fclose(in);
        return false;
    }
    char buf[64 * 1024];
    bool ok = true;
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), in)) > 0;) {
        if (std::fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    if (std::ferror(in)) ok = false;
    std::fclose(in);
    // Ошибку закрытия учитываем: буферизованная запись может отказать только на flush, и
    // «скопировали» с усечённым хвостом дало бы загрузку битого модуля.
    if (std::fclose(out) != 0) ok = false;
    if (!ok) remove_file(dst);
    return ok;
}

namespace {

std::string env_dir(const char* name, const std::string& suffix) {
    std::string base;
    if (!env_var(name, base) || base.empty()) return {};
    return is_absolute(base) ? base + suffix : std::string{};
}

} // namespace

std::string user_data_dir(const std::string& app_name) {
#if defined(__APPLE__)
    return env_dir("HOME", "/Library/Application Support/" + app_name);
#else
    const std::string xdg = env_dir("XDG_DATA_HOME", "/" + app_name);
    return xdg.empty() ? env_dir("HOME", "/.local/share/" + app_name) : xdg;
#endif
}

} // namespace platform
