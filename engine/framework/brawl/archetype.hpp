#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "clip_read.hpp"
#include "fighter.hpp"
#include "fighter_read.hpp"

namespace framework::brawl {

constexpr uint32_t MAX_MOVES = 16;

struct MoveSlot {
    uint16_t clip = 0;
    Strike strike;
};

struct Archetype {
    const graphics::ClipTable* clips = nullptr;
    DepthProfile profile{};
    fix32 depth{};
    uint16_t idle = 0, walk = 0, jump = 0;
    uint16_t hurt = 0, fall = 0, down = 0, getup = 0;
    uint16_t down_ticks = 0, getup_ticks = 0;
    std::array<MoveSlot, MAX_MOVES> moves{};
    uint32_t move_count = 0;
};

bool make_archetype(const FighterTable& fighter, const graphics::ClipTable& clips, Archetype& out,
                    std::string& error);
bool clip_index(const graphics::ClipTable& clips, const char* name, uint16_t& out);
bool is_move(const Archetype& a, uint16_t clip);
bool find_move(const Archetype& a, uint16_t clip, uint8_t box, uint8_t& slot);

} // namespace framework::brawl
