#include "assets_path.hpp"

#include <cstdio>
#include <cstdlib>

#include "platform_fs.hpp"

// Политика игры поверх платформенного шва (#12): ГДЕ искать ассеты и куда класть сейв.
// Как устроены пути, каталоги и юникод на конкретной ОС — знает engine/platform, не игра.
namespace game {
namespace {

std::string save_dir() {
    if (const char* env = std::getenv("LIKENES_SAVE_DIR")) {
        if (*env != '\0') return env;
    }
    return platform::user_data_dir("like-nes");
}

} // namespace

std::string resolve_asset(const char* name) {
    if (const char* env = std::getenv("LIKENES_ASSETS")) {
        std::string p = std::string(env) + "/" + name;
        if (platform::file_exists(p)) return p;
    }
    const std::string ed = platform::exe_dir();
    const std::string n = name;
    // Только exe-относительные кандидаты (+ env) → путь не зависит от cwd (детерм. вывод demo).
    const std::string candidates[] = {
        ed + "/" + n,                             // tarball / Windows-папка / .app Contents/MacOS
        ed + "/../Resources/" + n,                // macOS .app
        ed + "/assets/" + n,
        ed + "/../example_ugly_game/assets/" + n, // dev: exe в build/, source рядом
    };
    for (const std::string& c : candidates)
        if (platform::file_exists(c)) return c;
    return {};
}

std::string resolve_bundle_path() { return resolve_asset("game.bundle"); }

// Сейв пишется в пользовательский каталог, а не в CWD: у установленной сборки (.app, tarball,
// ярлык) рабочий каталог произвольный — относительный путь либо не записывался бы вовсе, либо
// разбрасывал прогресс по случайным папкам.
std::string resolve_save_path(const char* name) {
    const std::string dir = save_dir();
    if (!dir.empty()) {
        if (platform::ensure_dir(dir)) return dir + "/" + name;
        std::fprintf(stderr, "[game] save dir '%s' unusable, falling back to exe dir\n", dir.c_str());
    }
    const std::string ed = platform::exe_dir();
    return (ed.empty() ? std::string(".") : ed) + "/" + name;
}

} // namespace game
