#include <cstdio>
#include <string>

#include "level_parts.hpp"

namespace framework::scene {

namespace {

using tilemap::LayerKind;

std::string guid_text(uint64_t guid) {
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(guid));
    return hex;
}

bool add_guid(Level& l, uint64_t guid, LoadError& error) {
    for (uint32_t i = 0; i < l.texture_count; ++i)
        if (l.guids[i] == guid) return true;
    if (l.texture_count == LEVEL_TEXTURES)
        return load_fail(error, LoadFault::Capacity,
                         std::string("level ") + l.name + " needs more than " + std::to_string(LEVEL_TEXTURES) +
                             " textures");
    l.guids[l.texture_count++] = guid;
    return true;
}

bool load_texture(Level& l, uint32_t i, LoadError& error) {
    const asset::LookupFault f = asset::raw_rgba8(l.bundle, l.guids[i], l.pixels[i]);
    if (f == asset::LookupFault::Ok) return true;
    return load_fail(error, LoadFault::Texture,
                     std::string("level ") + l.name + ": texture " + guid_text(l.guids[i]) + ": " +
                         asset::lookup_fault_name(f));
}

} // namespace

bool load_textures(Level& level, LoadError& error) {
    for (const auto& ts : level.map.tilesets)
        if (!add_guid(level, ts.texture_guid, error)) return false;
    for (const auto& layer : level.map.layers)
        if (layer.kind == static_cast<uint8_t>(LayerKind::Image) && !add_guid(level, layer.image_guid, error))
            return false;
    for (uint32_t i = 0; i < level.texture_count; ++i)
        if (!load_texture(level, i, error)) return false;
    return true;
}

} // namespace framework::scene
