#pragma once
#include "fixed.hpp"

namespace framework::brawl {

struct DepthProfile {
    fix32 speed_x, speed_z, gravity, jump_vy;
};

} // namespace framework::brawl
