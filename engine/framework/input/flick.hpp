#pragma once
#include <cstdint>
#include <span>

#include "input_types.hpp"

namespace framework::input {

struct FlickRule {
    int hold = 0;
    int axis = 0;
    fix32 threshold;
    uint32_t ticks = 0;
};

bool flick(std::span<const ::input::InputFrame> recent, const FlickRule& rule);

} // namespace framework::input
