#pragma once
#include <cstdint>

namespace framework::brawl {

struct RunState {
    int8_t dir = 0;
    int8_t held = 0;
    int8_t tap = 0;
    uint8_t tap_ticks = 0;
};

} // namespace framework::brawl
