#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "hash_mix.hpp"
#include "object_bake.hpp"
#include "object_read.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::tilemap;

constexpr uint64_t GOLDEN = 0x05b8ddc47b75b807ull;

uint64_t hash_bytes(const std::vector<uint8_t>& b) {
    uint64_t h = framework::physics::FNV_OFFSET;
    framework::physics::mix_bytes(h, b.data(), b.size());
    return h;
}

ObjectSrc obj(uint32_t id, const char* name, const char* cls, ObjectShape shape, int x, int y) {
    ObjectSrc o;
    o.id = id;
    o.name = name;
    o.cls = cls;
    o.shape = shape;
    o.x = fix32::from_int(x);
    o.y = fix32::from_int(y);
    return o;
}

std::vector<ObjectMapSrc> source() {
    ObjectSrc door = obj(9, "door", "trigger", ObjectShape::Rect, 32, 48);
    door.w = fix32::from_int(16);
    door.h = fix32(98304, 0);
    door.props = {{"target", PropType::String, 0, "level2"}, {"locked", PropType::Bool, 1, ""}};
    ObjectSrc spawn = obj(3, "start", "spawn", ObjectShape::Point, -4, 8);
    spawn.props = {{"speed", PropType::Fix, 98304, ""}, {"lives", PropType::Int, -3, ""}};
    ObjectSrc pit = obj(5, "pit", "trigger", ObjectShape::Polygon, 0, 0);
    pit.vertices = {{0, 0}, {65536, 0}, {0, 65536}};
    ObjectSrc enemy = obj(4, "", "spawn", ObjectShape::Point, 100, 10);
    ObjectMapSrc one{"one", {door, spawn, pit, enemy}};
    ObjectMapSrc two{"two", {obj(1, "", "", ObjectShape::Point, 0, 0)}};
    return {one, two};
}

void test_round_trip(const std::vector<uint8_t>& bytes) {
    ObjectTable t;
    check(t.open(bytes.data(), bytes.size()), "baked table opens");
    check(t.count() == 2 && std::strcmp(t.name(1), "two") == 0, "both maps keep their names");
    const ObjectMap m = t.find("one");
    check(m.row != nullptr && t.find("absent").row == nullptr, "find hits by name and misses unknown");
    if (m.row == nullptr || m.objects.size() != 4) return;
    check(m.objects[0].id == 3 && m.objects[1].id == 4 && m.objects[2].id == 5 && m.objects[3].id == 9,
          "objects sorted by class, then id");
    const auto spawns = objects_of_class(m, "spawn");
    const auto triggers = objects_of_class(m, "trigger");
    check(spawns.size() == 2 && spawns[0].id == 3, "spawn class is one contiguous span");
    check(triggers.size() == 2 && triggers[1].id == 9, "trigger class is one contiguous span");
    check(objects_of_class(m, "absent").empty() && objects_of_class(m, "").empty(), "missing class is empty");
    const MapObject& start = spawns[0];
    check(start.shape == static_cast<uint8_t>(ObjectShape::Point) && start.x_raw == -4 * 65536 &&
              start.y_raw == 8 * 65536 && start.w_raw == 0 && start.h_raw == 0,
          "point keeps its position and has no size");
    check(std::strcmp(object_text(m, start.name_offset), "start") == 0, "object name survives");
    check(std::strcmp(object_text(m, spawns[1].name_offset), "") == 0, "unnamed object reads empty");
    const ObjectProp* speed = object_prop(m, start, "speed");
    const ObjectProp* lives = object_prop(m, start, "lives");
    check(speed != nullptr && speed->type == static_cast<uint8_t>(PropType::Fix) && speed->value == 98304,
          "fix property survives");
    check(lives != nullptr && lives->value == -3, "negative int property survives");
    check(object_prop(m, start, "absent") == nullptr, "missing property is null");
    const MapObject& door = triggers[1];
    check(door.w_raw == 16 * 65536 && door.h_raw == 98304, "rect keeps its size");
    const ObjectProp* target = object_prop(m, door, "target");
    check(target != nullptr && std::strcmp(object_text(m, static_cast<uint32_t>(target->value)), "level2") == 0,
          "string property points at its text");
    check(object_props(m, door).size() == 2, "props span covers the object");
    const auto poly = object_vertices(m, triggers[0]);
    check(poly.size() == 3 && poly[1].x_raw == 65536 && poly[2].y_raw == 65536, "polygon keeps its vertices");
    check(object_vertices(m, door).empty(), "rect has no vertices");
}

struct Bad {
    const char* what;
    std::function<void(std::vector<ObjectMapSrc>&)> edit;
    const char* message;
};

void test_bake_refusals() {
    const Bad cases[] = {
        {"duplicate id", [](auto& m) { m[0].objects[1].id = 9; }, "duplicate id"},
        {"duplicate map", [](auto& m) { m[1].name = "one"; }, "declared twice"},
        {"unnamed map", [](auto& m) { m[1].name.clear(); }, "needs a name"},
        {"polygon of two", [](auto& m) { m[0].objects[2].vertices.pop_back(); }, "allowed 3 to 16"},
        {"polygon of 17", [](auto& m) { m[0].objects[2].vertices.resize(17); }, "allowed 3 to 16"},
        {"vertices on a rect", [](auto& m) { m[0].objects[0].vertices = {{0, 0}}; }, "not a polygon"},
        {"property twice", [](auto& m) { m[0].objects[1].props[1].name = "speed"; }, "twice"},
    };
    for (const Bad& c : cases) {
        std::vector<ObjectMapSrc> src = source();
        c.edit(src);
        std::vector<uint8_t> out;
        std::string error;
        const bool ok = bake_objects(src, out, error);
        check(!ok && error.find(c.message) != std::string::npos, c.what);
        if (ok || error.find(c.message) == std::string::npos) std::printf("    got: %s\n", error.c_str());
    }
}

void test_reader_refusals(const std::vector<uint8_t>& bytes) {
    ObjectTable t;
    t.open(bytes.data(), bytes.size());
    const ObjectMap m = t.find("one");
    if (m.row == nullptr || m.objects.size() != 4) return;
    const auto at = [&](const void* p) {
        return static_cast<std::size_t>(static_cast<const uint8_t*>(p) - bytes.data());
    };
    const auto refused = [&](const char* what, std::size_t offset, std::vector<uint8_t> patch) {
        std::vector<uint8_t> bad = bytes;
        std::memcpy(bad.data() + offset, patch.data(), patch.size());
        ObjectTable r;
        check(!r.open(bad.data(), bad.size()), what);
    };
    const MapObject& start = m.objects[0];
    const ObjectProp* lp = object_prop(m, m.objects[3], "locked");
    if (lp == nullptr) return;
    const ObjectProp& locked = *lp;
    refused("wrong magic", 0, {'L', 'N', 'V', 'L'});
    refused("ids out of order", at(&m.objects[1].id), {2, 0, 0, 0});
    refused("unknown shape", at(&start.shape), {3});
    refused("point with a size", at(&start.w_raw), {1, 0, 0, 0});
    refused("polygon of two vertices", at(&m.objects[2].vertex_count), {2});
    refused("bool other than 0 or 1", at(&locked.value), {2, 0, 0, 0});
    refused("unknown property type", at(&locked.type), {4});
    refused("props past the table", at(&start.prop_count), {200, 0, 0, 0});
    std::vector<uint8_t> cut(bytes.begin(), bytes.end() - 16);
    ObjectTable r;
    check(!r.open(cut.data(), cut.size()), "truncated section");
}

} // namespace

int main() {
    std::vector<uint8_t> bytes;
    std::string error;
    check(bake_objects(source(), bytes, error), "object maps bake");
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    test_round_trip(bytes);
    const uint64_t h = hash_bytes(bytes);
    check(h == GOLDEN, "byte table matches the golden");
    if (h != GOLDEN) std::printf("    hash 0x%016llx\n", static_cast<unsigned long long>(h));
    test_bake_refusals();
    test_reader_refusals(bytes);
    std::printf("framework-tilemap-objects: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
