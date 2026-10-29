#pragma once
#include "fixed.hpp"

namespace framework::depth {

struct DepthBody {
    fix32 x, z, y, vx, vz, vy;
};

} // namespace framework::depth
