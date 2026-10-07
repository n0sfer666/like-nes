#pragma once
#include <cstdint>

namespace framework::brawl {

constexpr uint8_t FIGHTER_MAGIC[4] = {'L', 'N', 'F', 'T'};
constexpr uint32_t FIGHTER_VERSION = 1;
constexpr uint8_t STRIKE_HITS_DOWN = 1;

struct FighterRow {
    uint32_t name_offset;
    uint32_t sheet_offset;
    int32_t speed_x_raw;
    int32_t speed_z_raw;
    int32_t run_x_raw;
    int32_t gravity_raw;
    int32_t jump_vy_raw;
    int32_t depth_raw;
    uint32_t hp;
    uint32_t move_offset;
    uint32_t move_count;
};
static_assert(sizeof(FighterRow) == 44, "FighterRow layout pinned (zero-parse ABI)");

struct StrikeRow {
    uint32_t clip_offset;
    uint8_t box;
    uint8_t type;
    uint8_t flags;
    uint8_t pad;
    uint32_t damage;
    int32_t depth_raw;
    uint32_t hitstop;
    uint32_t hitstun;
    int32_t knock_x_raw;
    int32_t knock_y_raw;
};
static_assert(sizeof(StrikeRow) == 32, "StrikeRow layout pinned (zero-parse ABI)");

} // namespace framework::brawl
