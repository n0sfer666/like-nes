#pragma once
#include <array>
#include <cstdint>

#include "ent_id.hpp"
#include "fighter.hpp"

namespace framework::brawl {

constexpr uint32_t MAX_HIT_EVENTS = 64;

struct HitRules {
    bool friendly_fire = false;
};

struct HitEvent {
    EntId attacker;
    EntId target;
    uint8_t box = 0;
    fix32 knock_vx{};
    uint8_t kind = 0;
    uint8_t move = 0;
};

struct HitEvents {
    std::array<HitEvent, MAX_HIT_EVENTS> at{};
    uint32_t count = 0;
    uint32_t dropped = 0;

    void clear() {
        count = 0;
        dropped = 0;
    }
    void push(const HitEvent& e) {
        if (count < MAX_HIT_EVENTS) at[count++] = e;
        else ++dropped;
    }
};

} // namespace framework::brawl
