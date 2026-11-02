#include "level_load.hpp"

#include <cstring>
#include <string>

#include "hash.hpp"
#include "level_parts.hpp"
#include "map_read.hpp"

namespace framework::scene {

namespace {

template <class Table>
bool open_table(const Level& l, const char* name, Table& out, LoadError& error) {
    const uint8_t* data = nullptr;
    std::size_t size = 0;
    if (!level_table(l, name, data, size, error)) return false;
    if (out.open(data, size)) return true;
    return load_fail(error, LoadFault::BadTable, std::string(name) + " table does not open");
}

const char* visual_name(const tilemap::VisualTable& t, const char* name) {
    for (uint32_t i = 0; i < t.count(); ++i)
        if (std::strcmp(t.name(i), name) == 0) return t.name(i);
    return nullptr;
}

bool no_level(LoadError& error, const char* name, const char* table) {
    return load_fail(error, LoadFault::NoLevel, std::string("no level ") + name + " in the " + table + " table");
}

bool find_maps(Level& l, const char* name, LoadError& error) {
    const char* found = visual_name(l.table, name);
    if (found == nullptr) return no_level(error, name, "visual");
    l.map = l.table.find(name);
    tilemap::TileMapTable tiles;
    if (!open_table(l, "tilemap", tiles, error)) return false;
    l.collision = tiles.find(name);
    if (!l.collision) return no_level(error, name, "tilemap");
    l.object_map = l.object_table.find(name);
    if (l.object_map.row == nullptr) return no_level(error, name, "objects");
    l.name = found;
    return true;
}

fix32 px(uint32_t v) { return fix32::from_int(static_cast<int32_t>(v)); }

} // namespace

bool level_table(const Level& level, const std::string& table, const uint8_t*& data, std::size_t& size,
                 LoadError& error) {
    const asset::LookupFault f = asset::raw_table(level.bundle, asset::fnv1a_str(table.c_str()), data, size);
    if (f == asset::LookupFault::Ok) return true;
    return load_fail(error, LoadFault::NoTable, table + " table: " + asset::lookup_fault_name(f));
}

bool open_level(asset::BundleView bundle, const char* name, Level& out, LoadError& error) {
    out = Level{};
    error = LoadError{};
    if (name == nullptr) return load_fail(error, LoadFault::NoLevel, "level name is null");
    out.bundle = bundle;
    if (open_table(out, "visual", out.table, error) && open_table(out, "objects", out.object_table, error) &&
        find_maps(out, name, error)) {
        const auto& row = *out.map.row;
        out.bounds = {fix32{}, fix32{}, px(row.width * row.tile_size), px(row.height * row.tile_size)};
        return true;
    }
    out = Level{};
    return false;
}

bool load_clips(Level& level, LoadError& error) {
    error = LoadError{};
    return open_table(level, "clips", level.clips, error);
}

bool load_level(asset::BundleView bundle, const char* name, Level& out, LoadError& error) {
    if (!open_level(bundle, name, out, error)) return false;
    if (load_textures(out, error)) return true;
    out = Level{};
    return false;
}
} // namespace framework::scene
