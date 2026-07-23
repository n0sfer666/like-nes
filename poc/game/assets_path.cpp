#include "assets_path.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
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

} // namespace

std::string resolve_bundle_path() {
    if (const char* env = std::getenv("LIKENES_ASSETS")) {
        std::string p = std::string(env) + "/game.bundle";
        if (exists(p)) return p;
    }
    const std::string ed = exe_dir();
    // Только exe-относительные кандидаты (+ env) → путь не зависит от cwd (детерм. вывод demo).
    const std::string candidates[] = {
        ed + "/game.bundle",                  // tarball / Windows-папка / .app Contents/MacOS
        ed + "/../Resources/game.bundle",     // macOS .app
        ed + "/assets/game.bundle",
        ed + "/../game/assets/game.bundle",   // dev: exe в build/, source рядом
    };
    for (const std::string& c : candidates)
        if (exists(c)) return c;
    return {};
}

} // namespace game
