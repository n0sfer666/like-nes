#pragma once
#include <cstdint>
#include <optional>

#include "bundle_lookup.hpp"
#include "bundle_view.hpp"
#include "camera.hpp"
#include "clip_read.hpp"
#include "grid.hpp"
#include "object_read.hpp"
#include "visual_read.hpp"

namespace framework::scene {

constexpr uint32_t LEVEL_TEXTURES = 8;
struct Level {
    asset::BundleView bundle;
    const char* name = "";
    std::optional<tilemap::TileGrid> collision;
    tilemap::VisualTable table;
    tilemap::VisualMap map;
    tilemap::ObjectTable object_table;
    tilemap::ObjectMap object_map;
    graphics::ClipTable clips;
    uint32_t texture_count = 0;
    uint64_t guids[LEVEL_TEXTURES] = {};
    asset::RgbaView pixels[LEVEL_TEXTURES] = {};
    graphics::CameraBounds bounds{};
};

} // namespace framework::scene
