#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "object_bake.hpp"
#include "scene_read.hpp"

namespace scene_fixture {

using namespace framework::tilemap;

inline ObjectPropSrc int_prop(const char* name, int32_t v) { return {name, PropType::Int, v, ""}; }
inline ObjectPropSrc fix_prop(const char* name, int32_t raw) { return {name, PropType::Fix, raw, ""}; }
inline ObjectPropSrc text_prop(const char* name, const char* text) { return {name, PropType::String, 0, text}; }
inline ObjectPropSrc bool_prop(const char* name, bool v) { return {name, PropType::Bool, v ? 1 : 0, ""}; }

inline ObjectSrc point(uint32_t id, const char* name, const char* cls, int x, int y) {
    ObjectSrc o;
    o.id = id;
    o.name = name;
    o.cls = cls;
    o.shape = ObjectShape::Point;
    o.x = fix32::from_int(x);
    o.y = fix32::from_int(y);
    return o;
}

inline ObjectSrc rect(uint32_t id, const char* name, const char* cls, int x, int y, int w, int h) {
    ObjectSrc o = point(id, name, cls, x, y);
    o.shape = ObjectShape::Rect;
    o.w = fix32::from_int(w);
    o.h = fix32::from_int(h);
    return o;
}

inline ObjectSrc with(ObjectSrc o, std::vector<ObjectPropSrc> props) {
    o.props = std::move(props);
    return o;
}

inline ObjectSrc wave_point(uint32_t id, const char* name, int x, int y, int32_t arena, int32_t wave, const char* kind) {
    return with(point(id, name, "spawn", x, y),
                {int_prop("arena", arena), int_prop("wave", wave), text_prop("kind", kind)});
}

constexpr int32_t ONE = 65536;

inline ObjectMapSrc level() {
    ObjectSrc lift = with(point(16, "lift", "path", 950, 100), {fix_prop("speed", ONE / 2), bool_prop("loop", true)});
    lift.shape = ObjectShape::Polygon;
    lift.vertices = {{0, 0}, {0, -48 * ONE}, {16 * ONE, -48 * ONE}};
    ObjectSrc grunt2 = wave_point(8, "grunt2", 420, 260, 1, 0, "adler");
    grunt2.props.push_back(int_prop("players", 2));
    ObjectSrc swing = with(point(18, "swing", "rope", 1000, 40), {fix_prop("length", 64 * ONE)});
    swing.x = fix32::from_raw(1000 * ONE + ONE / 2);
    ObjectSrc rail = with(point(20, "rail", "path", 1100, 100), {fix_prop("speed", ONE), bool_prop("loop", false)});
    rail.shape = ObjectShape::Polygon;
    for (int32_t i = 0; i < 16; ++i) rail.vertices.push_back({i * ONE, (i % 2) * ONE});
    return {"lvl",
            {with(point(1, "player", "spawn", 200, 264), {text_prop("facing", "right")}),
             rect(2, "street", "bounds", 104, 56, 432, 224),
             rect(3, "walk", "depth_band", 104, 232, 432, 32),
             with(point(4, "rainbird", "spawn", 264, 240), {text_prop("facing", "left")}),
             with(rect(5, "a1", "arena", 104, 56, 384, 224), {int_prop("id", 1)}),
             with(rect(6, "a2", "arena", 488, 56, 384, 224), {int_prop("id", 2)}),
             wave_point(7, "grunt1", 400, 250, 1, 0, "adler"),
             grunt2,
             wave_point(9, "boss", 700, 250, 2, 1, "banderas"),
             with(rect(10, "street", "section", 104, 56, 768, 224), {text_prop("mode", "belt")}),
             with(rect(11, "roof", "section", 872, 0, 200, 280), {text_prop("mode", "platform")}),
             rect(12, "car", "wall", 300, 236, 40, 12),
             rect(13, "pit", "kill", 900, 270, 40, 10),
             with(point(14, "cp1", "checkpoint", 500, 250), {int_prop("order", 1)}),
             with(rect(15, "belt", "conveyor", 880, 200, 64, 8), {fix_prop("speed", ONE + ONE / 2)}),
             lift,
             with(rect(17, "climb", "autoscroll", 872, 0, 200, 280),
                  {text_prop("axis", "y"), fix_prop("speed", -ONE / 4)}),
             swing,
             with(rect(19, "neon", "light", 120, 60, 8, 8), {fix_prop("id", ONE), int_prop("mode", 3)}),
             rail}};
}

struct Baked {
    std::vector<uint8_t> bytes;
    ObjectTable table;
    ObjectMap map;
    std::string error;
};

inline bool bake(const ObjectMapSrc& src, Baked& out) {
    const ObjectMapSrc maps[] = {src};
    if (!bake_objects(std::span<const ObjectMapSrc>(maps), out.bytes, out.error)) return false;
    if (!out.table.open(out.bytes.data(), out.bytes.size())) return false;
    out.map = out.table.find(src.name.c_str());
    return out.map.row != nullptr;
}

} // namespace scene_fixture
