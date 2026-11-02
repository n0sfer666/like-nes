#pragma once
#include <cstdint>

#include "depth_floor.hpp"

namespace framework::brawl {

inline constexpr uint32_t MAX_WALLS = 16;
using depth::FloorRect;
using DepthFloor = depth::DepthFloor<MAX_WALLS>;
using depth::blocked;
using depth::move_x;
using depth::move_z;
using depth::slide;

} // namespace framework::brawl
