#pragma once
#include <array>
#include <cstdint>

#include "fixmath.hpp"
#include "object_format.hpp"

namespace framework::scene {

struct LevelRect {
    fix32 x0, y0, x1, y1;
};

enum class ScrollAxis : uint8_t { X, Y };

struct Area {
    const tilemap::MapObject* object = nullptr;
    LevelRect rect{};
};

struct Mark {
    const tilemap::MapObject* object = nullptr;
    Vec2 at{};
};

// Потолок скорости пути, px за тик: при 60 Гц это 1920 px/с, ниже потолка `MAX_SPEED` физики (2048),
// и тело пути приходит в свою точку за шаг. Ноль и меньше загрузчик тоже отказывает: стоящая
// платформа пишется путём из одной точки, а нулевая скорость — скорее опечатка, чем замысел.
inline constexpr int32_t PATH_SPEED_MAX = 32;

struct Path {
    const tilemap::MapObject* object = nullptr;
    std::array<Vec2, tilemap::MAX_POLYGON_VERTICES> points{};
    uint8_t count = 0;
    fix32 speed;
    bool loop = false;
    int32_t w = 0;
    int32_t h = 0;
};

struct Autoscroll {
    Area area;
    ScrollAxis axis = ScrollAxis::X;
    fix32 speed;
};

} // namespace framework::scene
