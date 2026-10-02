#pragma once
#include <string>
#include <vector>

#include "tmj_parts.hpp"
#include "visual_bake.hpp"

namespace framework::tiled {

bool read_tiles(const Doc& d, const json::Node& tileset, Tileset& ts, std::vector<tilemap::VisualAnimSrc>& anims,
                std::string& error);

} // namespace framework::tiled
