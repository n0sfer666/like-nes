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

// Ход по `x` из `x` в `to` при глубине `z`: стены и края полосы останавливают его. Общий для `slide` и
// поводка сим-камеры: поводок, писавший `x` в обход, вставлял тело внутрь стены, а `sweep` такое тело
// уже не держит — оно проходило стену насквозь.
fix32 move_x(const DepthFloor& f, fix32 x, fix32 z, fix32 to);

void slide(const DepthFloor& f, DepthBody& b);

} // namespace framework::brawl
