#pragma once
#include <cstdint>

namespace framework::brawl {

constexpr uint16_t NO_STRIKE = 0xffffu;
constexpr uint8_t NO_CHAIN = 0xffu;

struct StrikeQueue {
    uint16_t strike = NO_STRIKE;
    uint16_t ticks = 0;
};

} // namespace framework::brawl
