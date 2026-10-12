#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "bundle_lookup.hpp"
#include "bundle_view.hpp"
#include "camera.hpp"
#include "clip_read.hpp"
#include "fighter_read.hpp"
#include "grid.hpp"
#include "object_read.hpp"
#include "scene_objects.hpp"
#include "visual_read.hpp"

namespace framework::scene {

constexpr uint32_t LEVEL_TEXTURES = 8;
constexpr uint32_t LEVEL_FIGHTERS = 8;

struct LevelFighter {
    std::string_view kind;
    brawl::FighterTable table;
};

struct Level {
    asset::BundleView bundle;
    const char* name = "";
    std::optional<tilemap::TileGrid> collision;
    tilemap::VisualTable table;
    tilemap::VisualMap map;
    tilemap::ObjectTable object_table;
    tilemap::ObjectMap object_map;
    LevelObjects objects;
    graphics::ClipTable clips;
    std::array<LevelFighter, LEVEL_FIGHTERS> fighters{};
    uint32_t fighter_count = 0;
    uint32_t texture_count = 0;
    uint64_t guids[LEVEL_TEXTURES] = {};
    asset::RgbaView pixels[LEVEL_TEXTURES] = {};
    graphics::CameraBounds bounds{};
};

} // namespace framework::scene
