#pragma once
#include <cstddef>
#include <cstdint>

#include "fighter_format.hpp"
#include "section_format.hpp"

namespace framework::brawl::test {

enum class At { Header, Row, Strike };

struct Corruption {
    const char* what;
    At at;
    std::size_t offset;
    std::size_t width;
    uint32_t value;
};

const uint32_t OVER_SPEED = static_cast<uint32_t>(MAX_FIGHTER_SPEED.raw + 1);
const uint32_t UNDER_KNOCK = static_cast<uint32_t>(-MAX_FIGHTER_SPEED.raw - 1);
const uint32_t OVER_DEPTH = static_cast<uint32_t>(MAX_FIGHTER_DEPTH.raw + 1);

const Corruption CORRUPTIONS[] = {
    {"version 1", At::Header, offsetof(framework::core::SectionHeader, version), 4, 1},
    {"count 2", At::Header, offsetof(framework::core::SectionHeader, count), 4, 2},
    {"hp over the cap", At::Row, offsetof(FighterRow, hp), 4, MAX_HP + 1},
    {"zero hp", At::Row, offsetof(FighterRow, hp), 4, MIN_HP - 1},
    {"zero down", At::Row, offsetof(FighterRow, down_ticks), 4, 0},
    {"down over the cap", At::Row, offsetof(FighterRow, down_ticks), 4, MAX_HIT_TICKS + 1},
    {"zero getup", At::Row, offsetof(FighterRow, getup_ticks), 4, 0},
    {"getup over the cap", At::Row, offsetof(FighterRow, getup_ticks), 4, MAX_HIT_TICKS + 1},
    {"zero gravity", At::Row, offsetof(FighterRow, gravity_raw), 4, 0},
    {"gravity over the cap", At::Row, offsetof(FighterRow, gravity_raw), 4, OVER_SPEED},
    {"negative speed", At::Row, offsetof(FighterRow, speed_x_raw), 4, 0xffffffffu},
    {"speed over the cap", At::Row, offsetof(FighterRow, speed_x_raw), 4, OVER_SPEED},
    {"negative speed_z", At::Row, offsetof(FighterRow, speed_z_raw), 4, 0xffffffffu},
    {"speed_z over the cap", At::Row, offsetof(FighterRow, speed_z_raw), 4, OVER_SPEED},
    {"negative run_x", At::Row, offsetof(FighterRow, run_x_raw), 4, 0xffffffffu},
    {"run_x over the cap", At::Row, offsetof(FighterRow, run_x_raw), 4, OVER_SPEED},
    {"negative jump_vy", At::Row, offsetof(FighterRow, jump_vy_raw), 4, 0xffffffffu},
    {"jump_vy over the cap", At::Row, offsetof(FighterRow, jump_vy_raw), 4, OVER_SPEED},
    {"zero depth", At::Row, offsetof(FighterRow, depth_raw), 4, 0},
    {"depth over the cap", At::Row, offsetof(FighterRow, depth_raw), 4, OVER_DEPTH},
    {"name past the strings", At::Row, offsetof(FighterRow, name_offset), 4, 0xffffu},
    {"moves past the end", At::Row, offsetof(FighterRow, move_count), 4, 9999},
    {"zero buffer", At::Row, offsetof(FighterRow, buffer_ticks), 4, 0},
    {"buffer over the cap", At::Row, offsetof(FighterRow, buffer_ticks), 4, MAX_HIT_TICKS + 1},
    {"zero run_tap", At::Row, offsetof(FighterRow, run_tap_ticks), 4, 0},
    {"run_tap over the cap", At::Row, offsetof(FighterRow, run_tap_ticks), 4, MAX_HIT_TICKS + 1},
    {"zero dodge", At::Row, offsetof(FighterRow, dodge_ticks), 4, 0},
    {"dodge over the cap", At::Row, offsetof(FighterRow, dodge_ticks), 4, MAX_HIT_TICKS + 1},
    {"move name past the strings", At::Strike, offsetof(StrikeRow, name_offset), 4, 0xffffu},
    {"empty chain", At::Row, offsetof(FighterRow, chain_count), 4, 0},
    {"chain past the end", At::Row, offsetof(FighterRow, chain_offset), 4, 0xfffffff0u},
    {"box 4", At::Strike, offsetof(StrikeRow, box), 1, MAX_HIT_BOXES},
    {"type 5", At::Strike, offsetof(StrikeRow, type), 1, HIT_TYPE_LAST + 1u},
    {"unknown flag", At::Strike, offsetof(StrikeRow, flags), 1, 4},
    {"pad set", At::Strike, offsetof(StrikeRow, pad), 1, 1},
    {"damage over the cap", At::Strike, offsetof(StrikeRow, damage), 4, MAX_DAMAGE + 1},
    {"zero strike depth", At::Strike, offsetof(StrikeRow, depth_raw), 4, 0},
    {"strike depth over the cap", At::Strike, offsetof(StrikeRow, depth_raw), 4, OVER_DEPTH},
    {"hitstop over the cap", At::Strike, offsetof(StrikeRow, hitstop), 4, MAX_HIT_TICKS + 1},
    {"hitstun over the cap", At::Strike, offsetof(StrikeRow, hitstun), 4, MAX_HIT_TICKS + 1},
    {"knock_x over the cap", At::Strike, offsetof(StrikeRow, knock_x_raw), 4, OVER_SPEED},
    {"knock_y over the cap", At::Strike, offsetof(StrikeRow, knock_y_raw), 4, OVER_SPEED},
    {"knock_x under the floor", At::Strike, offsetof(StrikeRow, knock_x_raw), 4, UNDER_KNOCK},
    {"knock_y under the floor", At::Strike, offsetof(StrikeRow, knock_y_raw), 4, UNDER_KNOCK},
};

} // namespace framework::brawl::test
