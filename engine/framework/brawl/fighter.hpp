#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "depth_profile.hpp"
#include "fixed.hpp"

namespace framework::brawl {

enum class HitType : uint8_t { Light = 0, Heavy = 1, Launch = 2, Grab = 3, Throw = 4 };
constexpr uint8_t HIT_TYPE_LAST = static_cast<uint8_t>(HitType::Throw);

constexpr uint8_t MAX_HIT_BOXES = 4;
constexpr uint32_t MIN_HP = 1;
constexpr uint32_t MAX_HP = 9999;
constexpr uint32_t MAX_DAMAGE = 999;
constexpr uint32_t MAX_HIT_TICKS = 120;
constexpr uint32_t MAX_CHAIN = 8;
constexpr const char* CANCEL_EVENT = "cancel";
constexpr fix32 MAX_FIGHTER_SPEED = fix32::from_int(16);
constexpr fix32 MAX_FIGHTER_DEPTH = fix32::from_int(64);

constexpr uint16_t ticks16(uint32_t v) { return v < 0xffffu ? static_cast<uint16_t>(v) : uint16_t{0xffff}; }

struct Strike {
    uint8_t box = 0;
    HitType type = HitType::Light;
    bool hits_down = false;
    bool slides = false;
    uint32_t damage = 0;
    fix32 depth{};
    uint32_t hitstop = 0;
    uint32_t hitstun = 0;
    fix32 knock_x{};
    fix32 knock_y{};
};

struct MoveSpec {
    std::string name;
    std::string clip;
    Strike strike;
    int line = 0;
};

struct FighterSpec {
    std::string sheet;
    fix32 speed_x{}, speed_z{}, run_x{}, gravity{}, jump_vy{}, depth{};
    uint32_t hp = 0;
    uint32_t down = 0, getup = 0, buffer = 0, run_tap = 0;
    std::vector<std::string> chain;
    int sheet_line = 0, chain_line = 0;
    std::vector<MoveSpec> moves;

    DepthProfile profile() const { return DepthProfile{speed_x, speed_z, gravity, jump_vy}; }
};

} // namespace framework::brawl
