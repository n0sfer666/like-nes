#include "assets_path.hpp"

#include <cstdio>
#include <vector>

#include "input_types.hpp"
#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "profile_file.hpp"
#include "save.hpp"

namespace game {
namespace {
bool configured = false, imported = false, warned = false;
std::string source;
std::vector<std::string> names;
bool prepare() {
    if (!configured || save::file_path("achievements.save").empty()) return false;
    if (imported) return true;
    imported = save::migrate_legacy(source, names);
    if (!imported && !warned) std::fprintf(stderr, "[game] some legacy saves could not be moved; original files retained\n");
    warned = !imported;
    return imported;
}
}
bool init_save_paths() {
    configured = true; imported = warned = false;
    names = {"achievements.save"};
    for (int player = 0; player < input::MAX_PLAYERS; ++player)
        names.push_back(framework::input::profile_file("controls.txt", player));
    std::string env;
    platform::env_var("LIKENES_SAVE_DIR", env);
    // The old policy chose ONE root. An explicit env must not drain OS/exe profiles into it.
    source = env.empty() ? platform::user_data_dir("like-nes") : env;
    if (source.empty()) source = platform::exe_dir();
    return save::init("like-nes") && prepare();
}
std::string resolve_save_path(const char* name) { return name && prepare() ? save::file_path(name) : std::string{}; }
std::string achievement_save_path() { return resolve_save_path("achievements.save"); }
}
