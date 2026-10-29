#pragma once
#include "fixed.hpp"

namespace framework::depth {

struct DepthProfile {
    fix32 speed_x, speed_z, gravity, jump_vy;
};

} // namespace framework::depth
