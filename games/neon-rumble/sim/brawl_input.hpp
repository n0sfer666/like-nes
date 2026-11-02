#pragma once
#include <cstdint>

#include "character_state.hpp"

namespace framework::brawl {

namespace button {
constexpr uint16_t ATTACK = 1u << 0;
constexpr uint16_t JUMP = 1u << 1;
constexpr uint16_t GRAB = 1u << 2;
constexpr uint16_t BLOCK = 1u << 3;
constexpr uint16_t DODGE = 1u << 4;
constexpr uint16_t RUN = 1u << 5;
} // namespace button

struct BrawlInput {
    character::MoveInput move;
    uint16_t buttons = 0;
    bool present = false;
};

inline bool operator==(const BrawlInput& a, const BrawlInput& b) {
    return a.move == b.move && a.buttons == b.buttons && a.present == b.present;
}

} // namespace framework::brawl
