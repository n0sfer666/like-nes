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
// Ход по `z` — пара к `move_x`; публичный ради поводка автоскролла по `y`, который толкает тело belt
// по глубине.
fix32 move_z(const DepthFloor& f, fix32 x, fix32 z, fix32 to);
// Точка вне полосы или внутри стены (края замкнуты, как у `sweep`): выход из platform туда отказан.
bool blocked(const DepthFloor& f, fix32 x, fix32 z);

void slide(const DepthFloor& f, DepthBody& b);

} // namespace framework::brawl
