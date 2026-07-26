#include "assets_path.hpp"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace game {
namespace {

std::string exe_dir() {
#if defined(__APPLE__)
    uint32_t n = 0;
    _NSGetExecutablePath(nullptr, &n);
    std::vector<char> buf(n);
    if (_NSGetExecutablePath(buf.data(), &n) != 0) return {};
    // _NSGetExecutablePath не канонизирует (symlink/.. не раскрыты) → realpath.
    char real[PATH_MAX];
    std::string p(realpath(buf.data(), real) ? real : buf.data());
#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return {};
    std::string p(buf, n);
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(buf)) return {}; // усечение → ошибка
    std::string p(buf, static_cast<size_t>(n));
#endif
    const size_t slash = p.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

bool exists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

bool is_dir(const std::string& path) {
#if defined(_WIN32)
    const DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

// «Уже существует» — успех только если там каталог: файл с тем же именем иначе молча проходил бы
// дальше, и запись сейва падала бы уже без диагностики.
bool make_dir(const std::string& path) {
#if defined(_WIN32)
    if (CreateDirectoryA(path.c_str(), nullptr) != 0) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS && is_dir(path);
#else
    // 0700, а не 0755: XDG требует приватности для $XDG_DATA_HOME, и прогресс игрока на
    // многопользовательской машине иначе читает любой локальный пользователь.
    if (::mkdir(path.c_str(), 0700) == 0) return true;
    return errno == EEXIST && is_dir(path);
#endif
}

// Разделитель и форма корня — платформенные: на POSIX `\` и `:` — легальные символы имени, и
// трактовать их по-виндовому значило бы съедать настоящие каталоги (`//Library/...` при HOME=/).
bool is_sep(char c) {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

// Корень создавать нельзя и не нужно: "C:" — не каталог, а drive-relative путь, "\\server\share" —
// точка монтирования. mkdir по ним вернул бы ошибку и обрубил бы всю цепочку.
size_t root_len(const std::string& path) {
#if defined(_WIN32)
    if (path.size() >= 2 && is_sep(path[0]) && is_sep(path[1])) {
        size_t i = 2;
        // Сегмент — непустой: дубль разделителя между сервером и шарой иначе обрывал бы корень
        // внутри имени шары, и mkdir по нему вернул бы ACCESS_DENIED.
        for (int seg = 0; seg < 2 && i < path.size(); ++seg) {
            while (i < path.size() && is_sep(path[i])) ++i;
            while (i < path.size() && !is_sep(path[i])) ++i;
        }
        while (i < path.size() && is_sep(path[i])) ++i;
        return i;
    }
    const char d = path.empty() ? '\0' : path[0];
    const bool drive = path.size() >= 2 && path[1] == ':' &&
                       ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z'));
    if (drive) return path.size() >= 3 && is_sep(path[2]) ? 3 : 2;
#endif
    return path.empty() || !is_sep(path[0]) ? 0 : 1;
}

bool ensure_dir(const std::string& path) {
    const size_t root = root_len(path);
    size_t end = path.size();
    while (end > root && is_sep(path[end - 1])) --end;
    for (size_t i = root; i < end; ++i) {
        // Пустой компонент (дубль разделителя) каталогом не является: на "C:\\dir" префикс до
        // второго слэша — сам корень, и mkdir по нему обрубил бы цепочку с ACCESS_DENIED.
        if (!is_sep(path[i]) || is_sep(path[i - 1])) continue;
        if (!make_dir(path.substr(0, i))) return false;
    }
    // За корнем ничего нет: CreateDirectoryA("D:\\") даёт ACCESS_DENIED, а не ALREADY_EXISTS, и явно
    // заданный каталог сейвов молча уехал бы в фолбэк.
    if (end <= root) return is_dir(path);
    return make_dir(path.substr(0, end));
}

bool is_absolute(const std::string& p) {
#if defined(_WIN32)
    if (p.size() >= 2 && is_sep(p[0]) && is_sep(p[1])) return true;
    const char d = p.empty() ? '\0' : p[0];
    // "C:dir" — не абсолютный: у каждого диска свой текущий каталог, разрешает его CWD процесса.
    return p.size() >= 3 && p[1] == ':' && is_sep(p[2]) &&
           ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z'));
#else
    return !p.empty() && p[0] == '/';
#endif
}

// Относительный HOME/XDG_DATA_HOME отбрасывается: XDG прямо это предписывает, а путь сейва,
// зависящий от CWD, — ровно та беда, ради которой resolve_save_path и существует.
std::string env_dir(const char* name, const char* suffix) {
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0') return {};
    const std::string base(v);
    if (!is_absolute(base)) return {};
    return base + suffix;
}

std::string save_dir() {
    if (const char* env = std::getenv("LIKENES_SAVE_DIR")) {
        if (*env != '\0') return env;
    }
#if defined(_WIN32)
    return env_dir("APPDATA", "\\like-nes");
#elif defined(__APPLE__)
    return env_dir("HOME", "/Library/Application Support/like-nes");
#else
    const std::string xdg = env_dir("XDG_DATA_HOME", "/like-nes");
    return xdg.empty() ? env_dir("HOME", "/.local/share/like-nes") : xdg;
#endif
}

} // namespace

std::string resolve_asset(const char* name) {
    if (const char* env = std::getenv("LIKENES_ASSETS")) {
        std::string p = std::string(env) + "/" + name;
        if (exists(p)) return p;
    }
    const std::string ed = exe_dir();
    const std::string n = name;
    // Только exe-относительные кандидаты (+ env) → путь не зависит от cwd (детерм. вывод demo).
    const std::string candidates[] = {
        ed + "/" + n,                     // tarball / Windows-папка / .app Contents/MacOS
        ed + "/../Resources/" + n,        // macOS .app
        ed + "/assets/" + n,
        ed + "/../game/assets/" + n,      // dev: exe в build/, source рядом
    };
    for (const std::string& c : candidates)
        if (exists(c)) return c;
    return {};
}

std::string resolve_bundle_path() { return resolve_asset("game.bundle"); }

// Сейв пишется в пользовательский каталог, а не в CWD: у установленной сборки (.app, tarball,
// ярлык) рабочий каталог произвольный — относительный путь либо не записывался бы вовсе, либо
// разбрасывал прогресс по случайным папкам.
std::string resolve_save_path(const char* name) {
    const std::string dir = save_dir();
    if (!dir.empty()) {
        if (ensure_dir(dir)) return dir + "/" + name;
        std::fprintf(stderr, "[game] save dir '%s' unusable, falling back to exe dir\n", dir.c_str());
    }
    const std::string ed = exe_dir();
    return (ed.empty() ? std::string(".") : ed) + "/" + name;
}

} // namespace game
