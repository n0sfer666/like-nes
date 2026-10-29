#include "rumble_floor.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;

bool inside(const br::FloorRect& r, fix32 x, fix32 z) { return !(x < r.x0) && !(r.x1 < x) && !(z < r.z0) && !(r.z1 < z); }

bool in_wall(const br::FloorRect& r, fix32 x, fix32 z) { return r.x0 < x && x < r.x1 && r.z0 < z && z < r.z1; }

} // namespace

bool on_floor(const br::DepthFloor& floor, fix32 x, fix32 z) {
    if (!inside(floor.band, x, z)) return false;
    for (uint32_t i = 0; i < floor.wall_count; ++i)
        if (in_wall(floor.walls[i], x, z)) return false;
    return true;
}

} // namespace rumble
