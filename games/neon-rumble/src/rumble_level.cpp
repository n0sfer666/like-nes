#include "rumble_level.hpp"

#include <cstdio>

#include "hash.hpp"

namespace rumble {

namespace {

using framework::tilemap::LayerKind;

fix32 px(uint32_t v) { return fix32::from_int(static_cast<int32_t>(v)); }

bool add_guid(Level& l, uint64_t guid) {
    for (uint32_t i = 0; i < l.texture_count; ++i)
        if (l.guids[i] == guid) return true;
    if (l.texture_count == MAX_TEXTURES) {
        std::fprintf(stderr, "neon-rumble: level needs more than %u textures\n", MAX_TEXTURES);
        return false;
    }
    l.guids[l.texture_count++] = guid;
    return true;
}

// Текстура, которой нет в бандле или которая лежит не сырым RGBA8, — отказ загрузки, а не пустой
// слой: тихо невидимый тайлсет на экране неотличим от ошибки камеры.
bool load_texture(Level& l, uint32_t i) {
    const asset::LookupFault f = asset::raw_rgba8(l.bundle, l.guids[i], l.pixels[i]);
    if (f != asset::LookupFault::Ok) {
        std::fprintf(stderr, "neon-rumble: texture %016llx: %s\n",
                     static_cast<unsigned long long>(l.guids[i]), asset::lookup_fault_name(f));
        return false;
    }
    l.sizes[i] = {l.pixels[i].width, l.pixels[i].height};
    return true;
}

fix32 end(int32_t at, int32_t size) { return fix32::from_raw(fix32::sat(int64_t{at} + size)); }

bool read_bounds(Level& l) {
    const auto& row = *l.map.row;
    l.bounds = {fix32{}, fix32{}, px(row.width * row.tile_size), px(row.height * row.tile_size)};
    framework::tilemap::ObjectTable objects;
    if (!l.open_objects(objects)) return false;
    const auto found = framework::tilemap::objects_of_class(objects.find(l.name), "bounds");
    if (found.empty()) return true;
    if (found.size() > 1) {
        std::fprintf(stderr, "neon-rumble: level %s has two objects of class bounds\n", l.name);
        return false;
    }
    const framework::tilemap::MapObject& o = found.front();
    if (o.shape != static_cast<uint8_t>(framework::tilemap::ObjectShape::Rect)) {
        std::fprintf(stderr, "neon-rumble: level %s: bounds object must be a rectangle\n", l.name);
        return false;
    }
    l.bounds = {fix32::from_raw(o.x_raw), fix32::from_raw(o.y_raw), end(o.x_raw, o.w_raw), end(o.y_raw, o.h_raw)};
    return true;
}

} // namespace

bool Level::read_table(const char* table_name, const uint8_t*& data, size_t& size) const {
    const asset::LookupFault f = asset::raw_table(bundle, asset::fnv1a_str(table_name), data, size);
    if (f == asset::LookupFault::Ok) return true;
    std::fprintf(stderr, "neon-rumble: %s table: %s\n", table_name, asset::lookup_fault_name(f));
    return false;
}

bool Level::open_objects(framework::tilemap::ObjectTable& objects) const {
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!read_table("objects", data, size)) return false;
    if (objects.open(data, size)) return true;
    std::fprintf(stderr, "neon-rumble: objects table: does not open\n");
    return false;
}

bool Level::open(const std::string& path, const char* level_name) {
    name = "";
    texture_count = 0;
    if (!file.open(path) || !bundle.open(file.data(), file.size(), false)) {
        std::fprintf(stderr, "neon-rumble: cannot open %s\n", path.c_str());
        return false;
    }
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!read_table("visual", data, size)) return false;
    if (!table.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: visual table does not parse\n");
        return false;
    }
    map = table.find(level_name);
    if (map.row == nullptr) {
        std::fprintf(stderr, "neon-rumble: no level %s in the visual table\n", level_name);
        return false;
    }
    name = level_name;
    for (const auto& ts : map.tilesets)
        if (!add_guid(*this, ts.texture_guid)) return false;
    for (const auto& layer : map.layers)
        if (layer.kind == static_cast<uint8_t>(LayerKind::Image) && !add_guid(*this, layer.image_guid))
            return false;
    for (uint32_t i = 0; i < texture_count; ++i)
        if (!load_texture(*this, i)) return false;
    return read_bounds(*this);
}

} // namespace rumble
