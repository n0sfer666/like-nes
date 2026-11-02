#pragma once
#include <array>
#include <string_view>

#include "fighter_read.hpp"
#include "scene_level.hpp"
#include "viewport_settings.hpp"
#include "level_objects.hpp"

namespace rumble {

constexpr uint32_t LEVEL_FIGHTERS = 8;
struct LevelFighter {
    std::string_view kind;
    framework::brawl::FighterTable table;
};

struct Stage {
    framework::scene::Level level;
    framework::graphics::ViewportSettings viewport;
    framework::scene::LevelObjects objects;
    std::array<LevelFighter, LEVEL_FIGHTERS> fighters{};
    uint32_t fighter_count = 0;
};

} // namespace rumble
