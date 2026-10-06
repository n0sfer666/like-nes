#pragma once
#include <array>
#include <cstdint>

#include "brawl_body.hpp"

namespace framework::brawl {

struct FloorRect {
    fix32 x0, z0, x1, z1;
};

constexpr uint32_t MAX_WALLS = 16;

struct DepthFloor {
    FloorRect band;
    std::array<FloorRect, MAX_WALLS> walls{};
    uint32_t wall_count = 0;

    bool add_wall(const FloorRect& r);
};

void slide(const DepthFloor& f, DepthBody& b);

} // namespace framework::brawl
