#include "level_load.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bundle_writer.hpp"
#include "hash.hpp"
#include "map_bake.hpp"
#include "object_bake.hpp"
#include "visual_bake.hpp"

namespace {

using namespace framework;
namespace sc = scene;
namespace tm = tilemap;
int failures = 0;
void check(bool ok, const char* why) {
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", why); }
}

asset::AssetInput table(const char* name, std::vector<uint8_t> bytes) {
    const auto size = static_cast<uint32_t>(bytes.size());
    return {asset::fnv1a_str(name), asset::AssetType::Raw, asset::Codec::Raw,
            asset::Residency::Mmap, std::move(bytes), size};
}

bool bundle(bool missing_texture, asset::BundleView& view) {
    tm::ParsedMap grid;
    grid.name = "room";
    grid.width = 2;
    grid.height = 3;
    grid.tile_size = fix32::from_int(16);
    grid.flags.assign(6, 0);
    tm::VisualMapSrc visual;
    visual.name = "room";
    visual.width = 2;
    visual.height = 3;
    visual.tile_size = 16;
    if (missing_texture) {
        tm::VisualLayerSrc layer;
        layer.name = "backdrop";
        layer.kind = tm::LayerKind::Image;
        layer.image_guid = asset::fnv1a_str("absent");
        layer.image_w = layer.image_h = 16;
        visual.layers.push_back(layer);
    }
    tm::ObjectMapSrc objects;
    objects.name = "room";
    tm::ObjectSrc object;
    object.id = 1;
    object.name = "game-owned";
    object.cls = "spawn";
    objects.objects.push_back(object);
    std::vector<uint8_t> tiles, visuals, object_bytes;
    tm::MapBakeError map_error;
    std::string error;
    if (!tm::bake_maps(std::span(&grid, 1), tiles, map_error) ||
        !tm::bake_visuals(std::span(&visual, 1), visuals, error) ||
        !tm::bake_objects(std::span(&objects, 1), object_bytes, error)) return false;
    const auto bytes = asset::write_bundle({table("tilemap", std::move(tiles)),
        table("visual", std::move(visuals)), table("objects", std::move(object_bytes))});
    alignas(64) static std::array<uint8_t, 32768> storage;
    if (bytes.size() > storage.size()) return false;
    std::memcpy(storage.data(), bytes.data(), bytes.size());
    return view.open(storage.data(), bytes.size(), false);
}

bool empty(const sc::Level& l) {
    return !l.bundle.valid() && l.name[0] == '\0' && !l.collision && l.texture_count == 0 && !l.clips.valid();
}

using Loader = bool (*)(asset::BundleView, const char*, sc::Level&, sc::LoadError&);

bool prepares_without_textures(Loader load, asset::BundleView view) {
    sc::Level level;
    sc::LoadError error;
    return load(view, "room", level, error) && error.fault == sc::LoadFault::Ok && level.texture_count == 0;
}

bool resets_failure(Loader load, asset::BundleView view) {
    sc::Level level;
    sc::LoadError error;
    return !load(view, "room", level, error) && error.fault == sc::LoadFault::Texture && empty(level);
}

bool partial_load(asset::BundleView view, const char* name, sc::Level& level, sc::LoadError& error) {
    return sc::open_level(view, name, level, error) && sc::load_textures(level, error);
}

} // namespace

int main() {
    asset::BundleView view;
    sc::Level level;
    sc::LoadError error;
    check(bundle(false, view), "the SDK-only fixture bakes without fighters or clips");
    check(sc::load_level(view, "room", level, error) && level.bounds.max_x == fix32::from_int(32) &&
          level.bounds.max_y == fix32::from_int(48) && level.object_map.objects.size() == 1,
          "the complete SDK loader reads maps and textures but leaves game classes and clips to the caller");
    check(!sc::load_clips(level, error) && error.fault == sc::LoadFault::NoTable,
          "explicit clip loading reports the missing optional table");
    check(bundle(true, view), "the missing-texture fixture bakes");
    check(prepares_without_textures(sc::open_level, view) && !prepares_without_textures(sc::load_level, view),
          "control: eagerly loading textures breaks the preparation phase");
    check(resets_failure(sc::load_level, view) && !resets_failure(partial_load, view),
          "control: exposing the partially loaded state breaks the full loader failure contract");
    check(sc::open_level(view, "room", level, error) && error.fault == sc::LoadFault::Ok,
          "preparation leaves missing textures for the later phase");
    check(!sc::load_textures(level, error) && error.fault == sc::LoadFault::Texture,
          "the texture phase reports its own failure");
    check(!sc::load_level(view, "room", level, error) && error.fault == sc::LoadFault::Texture && empty(level),
          "a failed complete load resets all SDK state");
    check(!sc::open_level(view, nullptr, level, error) && error.fault == sc::LoadFault::NoLevel && empty(level),
          "a null level name resets a previously used level");
    std::printf("framework-scene-load: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
