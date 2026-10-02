#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "fixed.hpp"
#include "visual_format.hpp"

namespace framework::tilemap {

struct VisualAnimSrc {
    uint16_t index = 0;
    std::vector<VisualFrame> frames;
};

struct VisualLayerSrc {
    std::string name;
    LayerKind kind = LayerKind::Tile;
    uint8_t opacity = 255;
    uint8_t repeat = 0;
    fix32 parallax_x = fix32::from_int(1);
    fix32 parallax_y = fix32::from_int(1);
    fix32 offset_x;
    fix32 offset_y;
    std::vector<uint16_t> cells;
    uint64_t image_guid = 0;
    uint32_t image_w = 0;
    uint32_t image_h = 0;
};

struct VisualMapSrc {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tile_size = 0;
    uint32_t background_rgba = 0;
    std::vector<VisualTileset> tilesets;
    std::vector<VisualAnimSrc> anims;
    std::vector<VisualLayerSrc> layers;
};

bool bake_visuals(std::span<const VisualMapSrc> maps, std::vector<uint8_t>& out, std::string& error);

} // namespace framework::tilemap
