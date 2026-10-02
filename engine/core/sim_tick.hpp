#pragma once
#include <cstdint>

namespace sim {

constexpr uint32_t TICK_HZ = 60;

constexpr uint64_t ticks_from_ms(uint64_t ms) {
    const uint64_t ticks = (ms * TICK_HZ + 500) / 1000;
    return ticks == 0 ? 1 : ticks;
}

} // namespace sim
