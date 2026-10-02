#include <cstdint>
#include <string>
#include <vector>

#include "fuzz_target.hpp"
#include "object_bake.hpp"
#include "object_read.hpp"
#include "visual_bake.hpp"
#include "visual_read.hpp"

namespace fuzz {
namespace {

namespace tl = framework::tilemap;

std::vector<uint8_t> seed_visuals() {
    tl::VisualMapSrc m;
    m.name = "one";
    m.width = 2;
    m.height = 2;
    m.tile_size = 16;
    m.background_rgba = 0x112233FFu;
    m.tilesets = {{0xC1, 1, 4, 2, 0, 0, 0}, {0xD1, 5, 2, 2, 1, 1, 0}};
    m.anims = {{1, {{2, 3}, {5, 7}}}};
    tl::VisualLayerSrc sky;
    sky.name = "sky";
    sky.kind = tl::LayerKind::Image;
    sky.repeat = tl::REPEAT_X;
    sky.image_guid = 0x51;
    sky.image_w = 32;
    sky.image_h = 16;
    tl::VisualLayerSrc ground;
    ground.name = "ground";
    ground.opacity = 128;
    ground.parallax_x = fix32(32768, 0);
    ground.cells = {0, static_cast<uint16_t>(2 | tl::CELL_FLIP_H), 4, static_cast<uint16_t>(6 | tl::CELL_FLIP_D)};
    m.layers = {sky, ground};
    std::vector<uint8_t> bytes;
    std::string err;
    tl::bake_visuals({&m, 1}, bytes, err);
    return bytes;
}

bool read_visuals(const uint8_t* data, size_t size) {
    tl::VisualTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        const char* mname = t.name(i);
        consume_str(mname);
        const tl::VisualMap m = mname != nullptr ? t.find(mname) : t.map(i);
        if (m.row == nullptr) continue;
        consume_all(m.row->width, m.row->height, m.row->tile_size, m.row->background_rgba);
        for (const tl::VisualTileset& ts : m.tilesets) consume_all(ts.texture_guid, ts.first_index, ts.columns);
        for (const tl::VisualAnim& a : m.anims) consume_all(a.index, a.frame_count, a.first_frame, a.cycle_ticks);
        for (const tl::VisualFrame& f : m.frames) consume_all(f.index, f.ticks);
        for (const tl::VisualLayer& l : m.layers) {
            consume_str(tl::layer_name(m, l));
            consume_all(l.kind, l.opacity, l.repeat, l.parallax_x_raw, l.offset_y_raw, l.image_guid);
            for (uint16_t cell : tl::layer_cells(m, l)) {
                for (uint32_t tick = 0; tick < 12; tick += 5) consume(tl::visual_region(m, cell, tick));
                const tl::VisualTileset* ts = tl::tileset_of(m, cell & tl::CELL_INDEX);
                if (ts != nullptr) consume(ts->tile_count);
            }
        }
    }
    return true;
}

tl::ObjectSrc object(uint32_t id, const char* name, const char* cls, tl::ObjectShape shape) {
    tl::ObjectSrc o;
    o.id = id;
    o.name = name;
    o.cls = cls;
    o.shape = shape;
    o.x = fix32::from_int(static_cast<int32_t>(id) * 8);
    o.y = fix32::from_int(-4);
    return o;
}

std::vector<uint8_t> seed_objects() {
    tl::ObjectSrc door = object(2, "door", "trigger", tl::ObjectShape::Rect);
    door.w = fix32::from_int(16);
    door.h = fix32::from_int(32);
    door.props = {{"target", tl::PropType::String, 0, "two"}, {"locked", tl::PropType::Bool, 1, ""}};
    tl::ObjectSrc start = object(1, "start", "spawn", tl::ObjectShape::Point);
    start.props = {{"speed", tl::PropType::Fix, 98304, ""}, {"lives", tl::PropType::Int, 3, ""}};
    tl::ObjectSrc pit = object(3, "pit", "trigger", tl::ObjectShape::Polygon);
    pit.vertices = {{0, 0}, {65536, 0}, {0, 65536}};
    const tl::ObjectMapSrc m{"one", {door, start, pit}};
    std::vector<uint8_t> bytes;
    std::string err;
    tl::bake_objects({&m, 1}, bytes, err);
    return bytes;
}

bool read_objects(const uint8_t* data, size_t size) {
    tl::ObjectTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        const char* mname = t.name(i);
        consume_str(mname);
        const tl::ObjectMap m = mname != nullptr ? t.find(mname) : t.map(i);
        for (const tl::MapObject& o : m.objects) {
            const char* cls = tl::object_text(m, o.class_offset);
            consume_str(tl::object_text(m, o.name_offset));
            consume_str(cls);
            consume_all(o.id, o.shape, o.x_raw, o.y_raw, o.w_raw, o.h_raw);
            if (cls != nullptr) consume(tl::objects_of_class(m, cls).size());
            for (const tl::ObjectVertex& v : tl::object_vertices(m, o)) consume_all(v.x_raw, v.y_raw);
            for (const tl::ObjectProp& p : tl::object_props(m, o)) {
                const char* pname = tl::object_text(m, p.name_offset);
                consume_str(pname);
                consume_all(p.type, p.value);
                if (p.type == static_cast<uint8_t>(tl::PropType::String)) {
                    consume_str(tl::object_text(m, static_cast<uint32_t>(p.value)));
                }
                const tl::ObjectProp* by_name = pname != nullptr ? tl::object_prop(m, o, pname) : nullptr;
                if (by_name != nullptr) consume(by_name->name_offset);
            }
        }
    }
    return true;
}

const Target TARGETS[] = {
    {"visual", seed_visuals, read_visuals},
    {"objects", seed_objects, read_objects},
};

} // namespace

const Target* level_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
