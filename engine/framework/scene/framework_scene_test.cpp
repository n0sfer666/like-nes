#include <cstdio>
#include <string_view>

#include "framework_scene_fixture.hpp"

namespace {

using namespace scene_fixture;
namespace sc = framework::scene;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

fix32 px(int v) { return fix32::from_int(v); }

bool rect_is(const sc::LevelRect& r, int x0, int y0, int x1, int y1) {
    return r.x0 == px(x0) && r.y0 == px(y0) && r.x1 == px(x1) && r.y1 == px(y1);
}

bool at_is(framework::Vec2 v, int x, int y) { return v.x == px(x) && v.y == px(y); }

std::string_view name_of(const ObjectMap& map, const MapObject* o) {
    return o == nullptr ? "" : object_text(map, o->name_offset);
}

void test_areas(const ObjectMap& map, const sc::LevelObjects& l) {
    check(l.bounds.has_value() && rect_is(l.bounds->rect, 104, 56, 536, 280), "bounds keeps its rectangle");
    check(l.depth_bands.size() == 1 && rect_is(l.depth_bands[0].rect, 104, 232, 536, 264), "depth band read");
    check(l.walls.size() == 1 && rect_is(l.walls[0].rect, 300, 236, 340, 248), "wall read");
    check(l.kills.size() == 1 && name_of(map, l.kills[0].object) == "pit", "kill read with its object");
    check(l.sections.size() == 2 && l.sections[0].mode == sc::SectionMode::Belt &&
              l.sections[1].mode == sc::SectionMode::Platform && rect_is(l.sections[1].area.rect, 872, 0, 1072, 280),
          "sections keep mode and rectangle");
    check(l.arenas.size() == 2 && l.arenas[0].id == 1 && l.arenas[1].id == 2 &&
              rect_is(l.arenas[1].area.rect, 488, 56, 872, 280),
          "arenas keep id and rectangle");
    check(l.conveyors.size() == 1 && l.conveyors[0].speed.raw == ONE + ONE / 2, "conveyor speed read");
    check(l.autoscrolls.size() == 1 && l.autoscrolls[0].axis == sc::ScrollAxis::Y &&
              l.autoscrolls[0].speed.raw == -ONE / 4,
          "autoscroll keeps axis and negative speed");
}

void test_marks(const ObjectMap& map, const sc::LevelObjects& l) {
    check(l.players.size() == 2 && name_of(map, l.players[0].object) == "player" &&
              name_of(map, l.players[1].object) == "rainbird" && at_is(l.players[1].at, 264, 240),
          "spawns without wave are player points in id order");
    const ObjectProp* facing = l.players.empty() ? nullptr : object_prop(map, *l.players[0].object, "facing");
    check(facing != nullptr && std::string_view(object_text(map, static_cast<uint32_t>(facing->value))) == "right",
          "player point leaves facing to the game through its object");
    check(l.waves.size() == 3, "spawns with wave are wave points");
    if (l.waves.size() == 3) {
        check(l.waves[0].arena == 1 && l.waves[0].wave == 0 && l.waves[0].kind == "adler" && l.waves[0].players == 1,
              "wave point without players defaults to one");
        check(l.waves[1].players == 2 && at_is(l.waves[1].mark.at, 420, 260), "wave point keeps players");
        check(l.waves[2].arena == 2 && l.waves[2].wave == 1 && l.waves[2].kind == "banderas",
              "second wave points at the second arena");
    }
    check(l.checkpoints.size() == 1 && l.checkpoints[0].order == 1 && at_is(l.checkpoints[0].mark.at, 500, 250),
          "checkpoint keeps order");
    check(l.ropes.size() == 1 && l.ropes[0].length == px(64) && l.ropes[0].mark.at.x.raw == 1000 * ONE + ONE / 2,
          "rope keeps length and a fractional anchor");
    check(l.paths.size() == 2 && l.paths[0].count == 3 && l.paths[0].loop && l.paths[0].speed.raw == ONE / 2,
          "path keeps speed, loop and vertex count");
    if (l.paths.size() != 2) return;
    check(at_is(l.paths[0].points[0], 950, 100) && at_is(l.paths[0].points[1], 950, 52) &&
              at_is(l.paths[0].points[2], 966, 52),
          "path points are absolute");
    check(l.paths[1].count == 16 && !l.paths[1].loop && at_is(l.paths[1].points[15], 1115, 101),
          "path with sixteen vertices fills the array to its last point");
}

void test_level(const Baked& b) {
    sc::LevelObjects l;
    std::string error = "stale";
    check(sc::read_level_objects(b.map, l, error), "full level reads");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    check(error.empty(), "success clears the error");
    test_areas(b.map, l);
    test_marks(b.map, l);
    check(objects_of_class(b.map, "light").size() == 1, "unknown class stays in the map for the game");
    check(sc::read_level_objects(b.map, l, error) && l.players.size() == 2 && l.waves.size() == 3 &&
              l.arenas.size() == 2,
          "second read replaces the first instead of appending");
}

void test_level1_shape() {
    ObjectMapSrc src{"level1",
                     {with(point(1, "player", "spawn", 200, 264), {text_prop("facing", "right")}),
                      rect(2, "street", "bounds", 104, 56, 432, 224), rect(3, "walk", "depth_band", 104, 232, 432, 32),
                      with(point(4, "rainbird", "spawn", 264, 240), {text_prop("facing", "left")}),
                      with(point(5, "adler", "spawn", 328, 252), {text_prop("facing", "left")})}};
    Baked b;
    check(bake(src, b), "level1 shape bakes");
    sc::LevelObjects l;
    std::string error;
    check(sc::read_level_objects(b.map, l, error) && l.players.size() == 3 && l.waves.empty() && l.arenas.empty(),
          "level1 of Neon Rumble reads as three player points");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
}

void test_empty() {
    Baked b;
    check(bake(ObjectMapSrc{"empty", {}}, b), "empty map bakes");
    sc::LevelObjects l;
    l.players.push_back({});
    std::string error;
    check(sc::read_level_objects(b.map, l, error) && l.players.empty() && !l.bounds.has_value(),
          "empty map reads as an empty dictionary");
}

} // namespace

int main() {
    Baked b;
    check(bake(level(), b), "fixture level bakes");
    if (!b.error.empty()) std::printf("    %s\n", b.error.c_str());
    test_level(b);
    test_level1_shape();
    test_empty();
    std::printf("framework-scene: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
