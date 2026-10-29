#pragma once
#include <array>
#include <cstdint>
#include <span>

#include "depth_body.hpp"

namespace framework::depth {

struct FloorRect {
    fix32 x0, z0, x1, z1;
};

struct FloorView {
    FloorRect band;
    std::span<const FloorRect> walls;
};

template <uint32_t N = 16>
struct DepthFloor {
    FloorRect band;
    std::array<FloorRect, N> walls{};
    uint32_t wall_count = 0;

    bool add_wall(const FloorRect& r) {
        if (wall_count == N || r.x1 < r.x0 || r.z1 < r.z0) return false;
        walls[wall_count++] = r;
        return true;
    }

    FloorView view() const { return FloorView{band, std::span<const FloorRect>(walls.data(), wall_count)}; }
};

// Ход по `x` из `x` в `to` при глубине `z`: стены и края полосы останавливают его. Общий для `slide` и
// поводка сим-камеры: поводок, писавший `x` в обход, вставлял тело внутрь стены, а `sweep` такое тело
// уже не держит — оно проходило стену насквозь.
fix32 move_x(FloorView f, fix32 x, fix32 z, fix32 to);
// Ход по `z` — пара к `move_x`; публичный ради поводка автоскролла по `y`, который толкает тело belt
// по глубине.
fix32 move_z(FloorView f, fix32 x, fix32 z, fix32 to);
// Точка вне полосы или внутри стены (края замкнуты, как у `sweep`): выход из platform туда отказан.
bool blocked(FloorView f, fix32 x, fix32 z);

void slide(FloorView f, DepthBody& b);

template <uint32_t N>
fix32 move_x(const DepthFloor<N>& f, fix32 x, fix32 z, fix32 to) {
    return move_x(f.view(), x, z, to);
}

template <uint32_t N>
fix32 move_z(const DepthFloor<N>& f, fix32 x, fix32 z, fix32 to) {
    return move_z(f.view(), x, z, to);
}

template <uint32_t N>
bool blocked(const DepthFloor<N>& f, fix32 x, fix32 z) {
    return blocked(f.view(), x, z);
}

template <uint32_t N>
void slide(const DepthFloor<N>& f, DepthBody& b) {
    slide(f.view(), b);
}

} // namespace framework::depth
