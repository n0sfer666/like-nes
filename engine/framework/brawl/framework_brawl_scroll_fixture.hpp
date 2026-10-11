#pragma once
#include <cstdint>

#include "framework_brawl_section_fixture.hpp"

namespace framework::brawl::test {

inline framework::scene::Autoscroll lane(int32_t x0, int32_t x1, framework::scene::ScrollAxis axis, int32_t speed) {
    return {area(x0, 0, x1, 400), axis, at(speed)};
}

inline void run(Sections& w, uint32_t ticks) {
    for (uint32_t t = 0; t < ticks; ++t) w.step({});
}

inline void place(Sections& w, int32_t a, int32_t b) {
    w.s.player(0).pos.x = at(a);
    w.s.player(1).pos.x = at(b);
}

inline void belt_lane(Sections& w, int32_t speed) {
    w.s.level.autoscrolls.push_back(lane(800, 1600, framework::scene::ScrollAxis::X, speed));
    w.s.seat(0, 900);
    w.s.seat(1, 1000);
    w.s.camera.center.x = at(950);
}

} // namespace framework::brawl::test
