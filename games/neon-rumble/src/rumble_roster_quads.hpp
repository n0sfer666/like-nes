#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "rumble_fighter.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_layers.hpp"

namespace rumble {

framework::Vec2 screen_plane(const framework::brawl::Body& body);

FighterStats draw_roster(FighterQuads& quads, const Fighters& fighters, const Brawl& brawl, Layers& layers,
                         LayerStats& st, uint32_t level_textures, bool overlay);

} // namespace rumble
