#include "assets_path.hpp"
#include "save.hpp"

#include <cstdio>

namespace {
std::string env, os, exe, chosen, imported;
int os_calls, exe_calls, fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-paths: FAIL %s\n", why); } }
}
namespace platform {
bool env_var(const char*, std::string& out) { out = env; return !env.empty(); }
std::string user_data_dir(const std::string&) { ++os_calls; return os; }
std::string exe_dir() { ++exe_calls; return exe; }
}
namespace save {
bool init(const std::string& app) { check(app == "like-nes", "game app id"); return true; }
std::string file_path(const std::string& key) { return chosen + "/" + key; }
bool migrate_legacy(const std::string& source, std::span<const std::string> names) {
    check(imported.empty(), "only one legacy root per startup"); imported = source;
    const std::vector<std::string> expected{"achievements.save", "controls.txt", "controls-p2.txt", "controls-p3.txt", "controls-p4.txt"};
    check(std::vector<std::string>(names.begin(), names.end()) == expected, "all player profiles, unchanged names");
    return true;
}
}
int main() {
    env = "isolated-B"; os = "normal-A"; exe = "old-exe"; chosen = env;
    check(game::init_save_paths() && imported == env && os_calls == 0 && exe_calls == 0,
          "explicit desktop override never reads or moves OS/exe files");
    imported.clear(); chosen = "mobile-C";
    check(game::init_save_paths() && imported == env && os_calls == 0 && exe_calls == 0,
          "different shell destination imports only explicit old env, not OS files");
    imported.clear(); env.clear();
    check(game::init_save_paths() && imported == os && os_calls == 1 && exe_calls == 0, "default old root only");
    imported.clear(); os.clear();
    check(game::init_save_paths() && imported == exe && exe_calls == 1, "exe only when OS root unavailable");
    std::puts(fails ? "save-paths: FAIL" : "save-paths: PASS explicit override isolation and one actual legacy source");
    return fails ? 1 : 0;
}
