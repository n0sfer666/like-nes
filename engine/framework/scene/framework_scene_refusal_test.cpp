#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>

#include "framework_scene_fixture.hpp"

namespace {

using namespace scene_fixture;
namespace sc = framework::scene;

int fails = 0;
int cases = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

ObjectSrc& obj(ObjectMapSrc& m, uint32_t id) {
    const auto it = std::find_if(m.objects.begin(), m.objects.end(), [&](const ObjectSrc& o) { return o.id == id; });
    if (it != m.objects.end()) return *it;
    std::printf("  FAIL: fixture has no object id %u\n", id);
    std::exit(1);
}

void put(ObjectSrc& o, ObjectPropSrc p) {
    for (ObjectPropSrc& q : o.props)
        if (q.name == p.name) {
            q = std::move(p);
            return;
        }
    o.props.push_back(std::move(p));
}

void drop(ObjectSrc& o, const char* name) {
    std::erase_if(o.props, [&](const ObjectPropSrc& p) { return p.name == name; });
}

void as_point(ObjectSrc& o) {
    o.shape = ObjectShape::Point;
    o.w = o.h = fix32{};
}

void as_rect(ObjectSrc& o) {
    o.shape = ObjectShape::Rect;
    o.vertices.clear();
    o.w = o.h = fix32::from_int(8);
}

void as_polygon(ObjectSrc& o) {
    o.shape = ObjectShape::Polygon;
    o.w = o.h = fix32{};
    o.vertices = {{0, 0}, {ONE, 0}, {0, ONE}};
}

void expect(const ObjectMap& map, const char* what, const char* message) {
    sc::LevelObjects l;
    l.players.push_back({});
    std::string error;
    const bool ok = sc::read_level_objects(map, l, error);
    const bool own = !ok && error.find(message) != std::string::npos;
    check(own, what);
    if (!own) std::printf("    got: %s\n", ok ? "(read)" : error.c_str());
    check(ok || (l.players.empty() && l.arenas.empty() && l.paths.empty() && !l.bounds.has_value()), what);
    ++cases;
}

struct Bad {
    const char* what;
    std::function<void(ObjectMapSrc&)> edit;
    const char* message;
};

const Bad CASES[] = {
    {"arena as a point", [](auto& m) { as_point(obj(m, 5)); }, "arena 'a1' (id 5) is a point; class arena needs a rectangle"},
    {"bounds as a point", [](auto& m) { as_point(obj(m, 2)); }, "class bounds needs a rectangle"},
    {"section as a polygon", [](auto& m) { as_polygon(obj(m, 10)); }, "is a polygon; class section needs a rectangle"},
    {"player spawn as a rectangle", [](auto& m) { as_rect(obj(m, 1)); }, "is a rectangle; class spawn needs a point"},
    {"wave spawn as a polygon", [](auto& m) { as_polygon(obj(m, 7)); }, "class spawn needs a point"},
    {"path as a rectangle", [](auto& m) { as_rect(obj(m, 16)); }, "class path needs a polygon"},
    {"rope as a rectangle", [](auto& m) { as_rect(obj(m, 18)); }, "class rope needs a point"},
    {"arena id as fix", [](auto& m) { put(obj(m, 5), fix_prop("id", ONE)); }, "property 'id' is float; class arena needs int"},
    {"wave as fix", [](auto& m) { put(obj(m, 7), fix_prop("wave", 0)); }, "property 'wave' is float; class spawn needs int"},
    {"arena of a wave point as fix", [](auto& m) { put(obj(m, 7), fix_prop("arena", ONE)); }, "'arena' is float"},
    {"players as fix", [](auto& m) { put(obj(m, 8), fix_prop("players", 2 * ONE)); }, "'players' is float"},
    {"order as fix", [](auto& m) { put(obj(m, 14), fix_prop("order", ONE)); }, "'order' is float"},
    {"kind as int", [](auto& m) { put(obj(m, 7), int_prop("kind", 1)); }, "'kind' is int; class spawn needs string"},
    {"mode as int", [](auto& m) { put(obj(m, 10), int_prop("mode", 0)); }, "'mode' is int; class section needs string"},
    {"conveyor speed as int", [](auto& m) { put(obj(m, 15), int_prop("speed", 1)); }, "'speed' is int; class conveyor needs float"},
    {"loop as int", [](auto& m) { put(obj(m, 16), int_prop("loop", 1)); }, "'loop' is int; class path needs bool"},
    {"rope length as bool", [](auto& m) { put(obj(m, 18), bool_prop("length", true)); }, "'length' is bool"},
    {"wave point without arena", [](auto& m) { drop(obj(m, 7), "arena"); }, "grunt1' (id 7) needs int property 'arena'"},
    {"wave point without kind", [](auto& m) { drop(obj(m, 9), "kind"); }, "needs string property 'kind'"},
    {"section without mode", [](auto& m) { drop(obj(m, 11), "mode"); }, "needs string property 'mode'"},
    {"arena without id", [](auto& m) { drop(obj(m, 6), "id"); }, "a2' (id 6) needs int property 'id'"},
    {"checkpoint without order", [](auto& m) { drop(obj(m, 14), "order"); }, "needs int property 'order'"},
    {"conveyor without speed", [](auto& m) { drop(obj(m, 15), "speed"); }, "belt' (id 15) needs float property 'speed'"},
    {"path without speed", [](auto& m) { drop(obj(m, 16), "speed"); }, "lift' (id 16) needs float property 'speed'"},
    {"path without loop", [](auto& m) { drop(obj(m, 16), "loop"); }, "needs bool property 'loop'"},
    {"autoscroll without axis", [](auto& m) { drop(obj(m, 17), "axis"); }, "needs string property 'axis'"},
    {"autoscroll without speed", [](auto& m) { drop(obj(m, 17), "speed"); }, "climb' (id 17) needs float property 'speed'"},
    {"rope without length", [](auto& m) { drop(obj(m, 18), "length"); }, "needs float property 'length'"},
    {"unknown section mode", [](auto& m) { put(obj(m, 10), text_prop("mode", "side")); }, "'mode' is 'side'; allowed belt, platform"},
    {"unknown scroll axis", [](auto& m) { put(obj(m, 17), text_prop("axis", "z")); }, "'axis' is 'z'; allowed x, y"},
    {"players zero", [](auto& m) { put(obj(m, 8), int_prop("players", 0)); }, "'players' is 0; allowed 1 or more"},
    {"negative wave", [](auto& m) { put(obj(m, 7), int_prop("wave", -1)); }, "'wave' is -1; allowed 0 or more"},
    {"empty kind", [](auto& m) { put(obj(m, 9), text_prop("kind", "")); }, "boss' (id 9) property 'kind' is empty"},
    {"second bounds", [](auto& m) { m.objects.push_back(rect(30, "more", "bounds", 0, 0, 8, 8)); }, "more' (id 30) is a second bounds"},
    {"arena id repeated", [](auto& m) {
         put(obj(m, 6), int_prop("id", 1));
         put(obj(m, 9), int_prop("arena", 1));
     }, "a2' (id 6) repeats arena id 1 of object id 5"},
    {"wave point at a missing arena", [](auto& m) { put(obj(m, 9), int_prop("arena", 7)); }, "boss' (id 9) points at arena 7; the level has no arena"},
};

void run(const Bad& c) {
    ObjectMapSrc src = level();
    c.edit(src);
    Baked b;
    if (!bake(src, b)) {
        check(false, c.what);
        std::printf("    fixture does not bake: %s\n", b.error.c_str());
        return;
    }
    expect(b.map, c.what, c.message);
}

void test_vertex_overflow() {
    Baked b;
    if (!bake(level(), b)) return check(false, "level bakes for the vertex overflow");
    std::vector<MapObject> objects(b.map.objects.begin(), b.map.objects.end());
    const std::vector<ObjectVertex> vertices(MAX_POLYGON_VERTICES + 1, ObjectVertex{0, 0});
    for (MapObject& o : objects)
        if (o.id == 16) {
            o.first_vertex = 0;
            o.vertex_count = static_cast<uint8_t>(vertices.size());
        }
    ObjectMap map = b.map;
    map.objects = objects;
    map.vertices = vertices;
    expect(map, "path past the vertex array of a hand-made map", "lift' (id 16) has 17 vertices; allowed 16 at most");
}

} // namespace

int main() {
    Baked b;
    sc::LevelObjects l;
    std::string error;
    check(bake(level(), b) && sc::read_level_objects(b.map, l, error), "base level reads before any edit");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    for (const Bad& c : CASES) run(c);
    test_vertex_overflow();
    std::printf("framework-scene-refusal: %s - %d cases\n", fails == 0 ? "PASS" : "FAIL", cases);
    return fails == 0 ? 0 : 1;
}
