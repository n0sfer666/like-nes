#include "input_setup.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "asset_manager.hpp"
#include "assets_path.hpp"
#include "hash.hpp"

namespace game {
namespace {

const char* PRESET_NAME = "default";
const char* PROFILE_FILE = "controls.txt";

bool read_preset_asset(const std::string& bundle_path, std::vector<uint8_t>& out) {
    asset::AssetManager am;
    if (!am.open(bundle_path, 256u * 1024, /*trusted=*/false)) return false;
    const uint64_t g = asset::fnv1a("input", std::strlen("input"));
    am.request(g);
    for (int f = 0; f < 500; ++f) {
        am.sync_point();
        if (am.is_ready(g)) break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    if (!am.is_ready(g)) {
        am.close();
        return false;
    }
    const asset::Loaded a = am.get(g);
    out.assign(static_cast<const uint8_t*>(a.data), static_cast<const uint8_t*>(a.data) + a.size);
    am.close();
    return true;
}

} // namespace

bool load_controls(Controls& out) {
    const std::string bundle = resolve_bundle_path();
    if (bundle.empty() || !read_preset_asset(bundle, out.blob)) {
        std::fprintf(stderr, "[game] controls: no input preset in bundle → no controls\n");
        return false;
    }
    if (!out.table.open(out.blob.data(), out.blob.size())) {
        std::fprintf(stderr, "[game] controls: input preset table is corrupt\n");
        return false;
    }
    const int preset = out.table.find_preset(PRESET_NAME);
    if (preset < 0 || !out.table.bind(static_cast<uint32_t>(preset), out.map)) {
        std::fprintf(stderr, "[game] controls: preset '%s' missing\n", PRESET_NAME);
        return false;
    }

    // Накладка игрока применяется только к ТОМУ пресету, для которого записана: чужие перебинды
    // легли бы на другие имена действий и переставили бы половину кнопок.
    std::string profile_preset;
    const std::string path = resolve_save_path(PROFILE_FILE);
    if (out.rebinds.load(path, profile_preset)) {
        if (profile_preset == PRESET_NAME)
            out.rebinds.apply(out.table, static_cast<uint32_t>(preset), out.map);
        else
            out.rebinds.reset_all();
    }

    input::PlayerAssign pa;
    pa.use_kbd_mouse = true;
    pa.pad_slot = 0;
    out.map.assign_player(0, pa);
    return true;
}

} // namespace game
