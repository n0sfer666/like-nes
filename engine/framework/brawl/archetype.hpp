#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "clip_read.hpp"
#include "fighter.hpp"
#include "fighter_read.hpp"

namespace framework::brawl {

constexpr uint32_t MAX_MOVES = 16;
constexpr uint32_t NO_CANCEL = 0xffffffffu;

struct MoveSlot {
    uint16_t clip = 0;
    uint16_t head = 0;
    const char* name = "";
    Strike strike;
};

struct Archetype {
    const graphics::ClipTable* clips = nullptr;
    DepthProfile profile{};
    fix32 run_x{};
    fix32 depth{};
    uint16_t idle = 0, walk = 0, jump = 0;
    uint16_t hurt = 0, fall = 0, down = 0, getup = 0;
    uint16_t down_ticks = 0, getup_ticks = 0;
    std::array<MoveSlot, MAX_MOVES> moves{};
    uint32_t move_count = 0;
    std::array<uint16_t, MAX_CHAIN> chain{};
    std::array<uint32_t, MAX_CHAIN> cancel{};
    uint8_t chain_count = 0;
    uint16_t buffer_ticks = 1;
    uint8_t run_tap = 1;
};

bool make_archetype(const FighterTable& fighter, const graphics::ClipTable& clips, Archetype& out,
                    std::string& error);
bool clip_index(const graphics::ClipTable& clips, const char* name, uint16_t& out);
bool find_named(const Archetype& a, const char* name, uint16_t& move);
bool find_move(const Archetype& a, uint16_t move, uint8_t box, uint8_t& slot);

} // namespace framework::brawl
