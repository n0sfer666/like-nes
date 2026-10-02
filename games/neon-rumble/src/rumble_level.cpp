#include "rumble_level.hpp"

#include <cstdio>

#include "hash.hpp"

namespace rumble {

namespace {

using framework::tilemap::LayerKind;

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

} // namespace

bool Level::open(const std::string& path, const char* level_name) {
    name = "";
    texture_count = 0;
    if (!file.open(path) || !bundle.open(file.data(), file.size(), false)) {
        std::fprintf(stderr, "neon-rumble: cannot open %s\n", path.c_str());
        return false;
    }
    const uint8_t* data = nullptr;
    size_t size = 0;
    const asset::LookupFault f = asset::raw_table(bundle, asset::fnv1a_str("visual"), data, size);
    if (f != asset::LookupFault::Ok) {
        std::fprintf(stderr, "neon-rumble: visual table: %s\n", asset::lookup_fault_name(f));
        return false;
    }
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
    return true;
}

} // namespace rumble
