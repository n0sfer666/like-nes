#include "assets_path.hpp"

#include "platform_env.hpp"
#include "platform_fs.hpp"

// Политика игры поверх платформенного шва (#12): ГДЕ искать ассеты и куда класть сейв.
// Как устроены пути, каталоги и юникод на конкретной ОС — знает engine/platform, не игра.
namespace game {

std::string resolve_asset(const char* name) {
    std::string env;
    if (platform::env_var("LIKENES_ASSETS", env)) {
        const std::string p = env + "/" + name;
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

} // namespace game
