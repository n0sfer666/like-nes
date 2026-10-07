#pragma once
#include <array>
#include <cstdint>

#include "archetype.hpp"
#include "rumble_fighter.hpp"
#include "rumble_roster.hpp"

namespace rumble {

struct Level;

struct PlayerMoves {
    uint16_t jab = 0;
    uint16_t cross = 0;
    uint16_t kick = 0;
    uint16_t jump_kick = 0;
};

struct Kinds {
    std::array<framework::brawl::Archetype, FIGHTERS> types{};
    std::array<int32_t, FIGHTERS> hp{};
    PlayerMoves player;

    bool open(const Level& level, const Fighters& fighters);
};

} // namespace rumble
