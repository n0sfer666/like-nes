#pragma once
#include <cstdint>
#include <span>

#include "input_types.hpp"

namespace framework::brawl {

struct FlickRule {
    int block = 0;
    int axis = 0;
    fix32 threshold;
    uint32_t ticks = 0;
};

bool dodge_flick(std::span<const ::input::InputFrame> recent, const FlickRule& rule);

} // namespace framework::brawl
