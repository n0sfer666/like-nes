#pragma once
#include "fixed.hpp"

namespace game {

struct StickAxis { fix32 x, y; };

inline StickAxis stick_axis(double dx, double dy, double radius) {
    if (radius <= 0.0) return {};
    double nx = dx / radius, ny = dy / radius;
    const double m = nx * nx + ny * ny;
    if (m > 1.0) {
        const double inv = 1.0 / __builtin_sqrt(m);
        nx *= inv; ny *= inv;
    }
    return {fix32::from_float(nx), fix32::from_float(ny)};
}

} // namespace game
