#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "rumble_scene_level_fixture.hpp"

namespace {

namespace sc = framework::scene;
using namespace level_fixture;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

bool same_name(const rumble::Stage& l, const char* name) { return std::strcmp(l.level.name, name) == 0; }

bool same_bounds(const framework::graphics::CameraBounds& b, int x0, int y0, int x1, int y1) {
    return b.min_x == fix32::from_int(x0) && b.min_y == fix32::from_int(y0) && b.max_x == fix32::from_int(x1) &&
           b.max_y == fix32::from_int(y1);
}

void test_level(const asset::BundleView& bundle, rumble::Stage& l) {
    rumble::StageError e;
    check(rumble::load_stage(bundle, "lvl", l, e), "level lvl loads");
    if (e.fault != rumble::StageFault::Ok) std::printf("    %s: %s\n", rumble::stage_fault_name(e.fault), e.text);
    check(e.fault == rumble::StageFault::Ok && e.text[0] == '\0', "success leaves the error clean");
    check(same_name(l, "lvl"), "name of the loaded level");
    check(l.level.collision && l.level.collision->width() == 4 && l.level.collision->height() == 2, "LNTM grid of lvl, not two");
    check(l.level.map.row != nullptr && l.level.map.row->width == 4 && l.level.map.layers.size() == 2, "LNVL map of lvl");
    check(l.level.object_map.row != nullptr && l.objects.waves.size() == 3, "LNOB objects of lvl read");
    check(l.level.texture_count == 2 && l.level.guids[0] == STREET && l.level.guids[1] == SKY, "tileset first, image layer next");
    check(l.level.pixels[0].width == 32 && l.level.pixels[0].height == 32 && l.level.pixels[1].width == 64 &&
              l.level.pixels[1].height == 32 && l.level.pixels[1].pixels != nullptr,
          "texture pixels and sides from the bundle");
    check(same_bounds(l.level.bounds, 104, 56, 536, 280), "bounds from the bounds object");
    check(l.level.clips.valid() && l.level.clips.find("adler/jab").row != nullptr, "LNCL opened");
    check(l.fighter_count == 2, "one fighter per distinct spawn kind");
    check(l.fighters[0].kind == "adler" && l.fighters[1].kind == "banderas", "kinds in spawn order");
    check(l.fighters[0].table.valid() && std::strcmp(l.fighters[0].table.sheet(), "adler") == 0 &&
              std::strcmp(l.fighters[1].table.sheet(), "banderas") == 0,
          "LNFT of each kind opened");
}

void test_other(rumble::Stage& l) {
    rumble::StageError e;
    rumble::stage_fail(e, rumble::StageFault::NoLevel, "stale");
    check(rumble::load_stage(l.level.bundle, "two", l, e), "level two loads into a used Level from its own bundle");
    check(e.fault == rumble::StageFault::Ok && e.text[0] == '\0', "success clears a used error");
    check(same_name(l, "two") && l.level.collision && l.level.collision->width() == 3, "second level chosen by name");
    check(same_bounds(l.level.bounds, 0, 0, 48, 48), "no bounds object: bounds are the map");
    check(l.level.texture_count == 1 && l.level.guids[0] == STREET && l.level.guids[1] == 0, "textures of lvl do not stay");
    check(l.fighter_count == 0 && l.fighters[0].kind.empty(), "fighters of lvl do not stay");
    check(l.objects.waves.empty() && l.objects.players.size() == 1, "objects of two");
    check(!l.level.clips.valid(), "a level without waves does not open LNCL");
}

bool load(const Source& s, const char* name, rumble::Stage& l, const char* drop = nullptr) {
    std::vector<asset::AssetInput> in;
    std::string error;
    asset::BundleView bundle;
    rumble::StageError e;
    if (!bake(s, in, error)) return false;
    if (drop != nullptr)
        std::erase_if(in, [&](const asset::AssetInput& a) { return a.guid == asset::fnv1a_str(drop); });
    return open(std::move(in), bundle) && rumble::load_stage(bundle, name, l, e);
}

void test_edges() {
    rumble::Stage l;
    Source s = source();
    add_images(s, 6);
    check(load(s, "lvl", l) && l.level.texture_count == sc::LEVEL_TEXTURES && l.level.pixels[7].width == 1,
          "eight textures, the capacity, load");
    s = source();
    add_kinds(s, 6);
    check(load(s, "lvl", l) && l.fighter_count == rumble::LEVEL_FIGHTERS && l.fighters[7].table.valid(),
          "eight fighter kinds, the capacity, load");
    s = source();
    add_image(s, "street again", STREET);
    check(load(s, "lvl", l) && l.level.texture_count == 2, "a guid of a tileset and an image layer loads once");
    check(load(source(), "two", l, "clips"), "a level without waves loads from a bundle without LNCL");
}

} // namespace

int main() {
    std::printf("level loads from a bundle by name\n");
    std::vector<asset::AssetInput> in;
    std::string error;
    asset::BundleView bundle;
    if (!bake(source(), in, error) || !open(std::move(in), bundle)) {
        std::printf("  FAIL: fixture bundle: %s\n", error.c_str());
        std::printf("framework-scene-level: FAIL\n");
        return 1;
    }
    rumble::Stage level;
    test_level(bundle, level);
    test_other(level);
    test_edges();
    std::printf("framework-scene-level: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
