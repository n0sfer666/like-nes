#pragma once
#include <array>

#include "clip_debug.hpp"
#include "fighter.hpp"
#include "rumble_fighter.hpp"

namespace rumble {

struct DepthOverlay {
    fix32 lift{};
    fix32 body{};
    std::array<fix32, framework::brawl::MAX_HIT_BOXES> hit{};
};

void draw_depth(framework::graphics::DebugDraw& dd, const Pose& pose, const framework::graphics::CelPlace& at,
                const DepthOverlay& depth);

} // namespace rumble
