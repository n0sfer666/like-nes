#include "input_setup.hpp"

#include <cstdio>
#include <string>

#include "assets_path.hpp"
#include "bundle_asset.hpp"
#include "player_layout.hpp"
#include "profile_file.hpp"

namespace game {
namespace {

const char* PRESET_NAME = "default";
const char* PROFILE_FILE = "controls.txt";

} // namespace

bool load_controls(Controls& out, const std::string& bundle_path) {
    const std::string bundle = bundle_path.empty() ? resolve_bundle_path() : bundle_path;
    if (bundle.empty() || !read_bundle_asset(bundle, "input", out.blob)) {
        std::fprintf(stderr, "[game] controls: no input preset in bundle -> no controls\n");
        return false;
    }
    if (!out.table.open(out.blob.data(), out.blob.size())) {
        std::fprintf(stderr, "[game] controls: input preset table is corrupt\n");
        return false;
    }
    const int preset = out.table.find_preset(PRESET_NAME);
    if (preset < 0) {
        std::fprintf(stderr, "[game] controls: preset '%s' missing\n", PRESET_NAME);
        return false;
    }

    // Накладка игрока применяется только к ТОМУ пресету, для которого записана: чужие перебинды
    // легли бы на другие имена действий и переставили бы половину кнопок.
    std::string profile_preset;
    const std::string path = resolve_save_path(framework::input::profile_file(PROFILE_FILE, 0).c_str());
    if (!out.rebinds.load(path, profile_preset) || profile_preset != PRESET_NAME) out.rebinds.reset_all();
    std::string error;
    if (!framework::input::build_layout(out.table, static_cast<uint32_t>(preset), out.rebinds, out.map, 0, error)) {
        std::fprintf(stderr, "[game] controls: %s\n", error.c_str());
        return false;
    }

    if (!out.map.assign_player(0, {.pad_slot = 0, .use_kbd_mouse = true, .use_touch = true})) {
        std::fprintf(stderr, "[game] controls: devices were not assigned\n");
        return false;
    }
    return true;
}

} // namespace game
