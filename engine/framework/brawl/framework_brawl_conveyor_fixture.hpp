#pragma once
#include <cstdint>
#include <vector>

#include "framework_brawl_section_fixture.hpp"

namespace framework::brawl::test {

inline scene::Conveyor lane(fix32 x0, fix32 y0, fix32 x1, fix32 y1, int32_t speed) {
    return {scene::Area{nullptr, {x0, y0, x1, y1}}, at(speed)};
}

inline scene::Conveyor lane(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t speed) {
    return lane(at(x0), at(y0), at(x1), at(y1), speed);
}

// Тело на уровне с секциями и его ход за тики: обе половины теста конвейера судят по нему.
struct Ride {
    Sections w;
    EntId id;
    Ride(int32_t x, int32_t z, const std::vector<Solid>& solids = {}) : w(solids), id(w.body(x, z)) {}
    Body& body() { return w.get(id); }
    fix32 x() { return body().pos.x; }
    void step(uint32_t ticks) {
        for (uint32_t t = 0; t < ticks; ++t) w.step({});
    }
    fix32 moved(uint32_t ticks) {
        const fix32 from = x();
        step(ticks);
        return x() - from;
    }
};

} // namespace framework::brawl::test
