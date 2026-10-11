#pragma once
#include <cstdint>

#include "framework_brawl_hit_fixture.hpp"
#include "scene_objects.hpp"
#include "stage_step.hpp"

namespace framework::brawl::test {

constexpr fix32 at(int32_t v) { return fix32::from_int(v); }

inline scene::Area area(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    return scene::Area{nullptr, {at(x0), at(y0), at(x1), at(y1)}};
}

// Сцена камеры: игроки — места 0 и 1 на одной полосе, уровень — только то, что положил случай.
struct Stage {
    Arena arena;
    BodyPool pool;
    Seats seats;
    StageCamera camera;
    StageSections sections;
    scene::LevelObjects level;

    Body& seat(uint32_t s, int32_t x) {
        const EntId id = pool.spawn(dummy(x, 240, 1, 0, arena.kinds[0]));
        seats.at[s] = Seat{true, id};
        return *pool.find(id);
    }

    Body& player(uint32_t s) { return *pool.find(seats.at[s].body); }

    BrawlWorld world() const {
        BrawlWorld w = arena.world();
        w.level = &level;
        return w;
    }

    void camera_step() { step_camera(camera, pool, seats, sections, world()); }
};

inline BrawlInput walk(int32_t dir) {
    BrawlInput in;
    in.present = true;
    in.move.move_x = at(dir);
    return in;
}

} // namespace framework::brawl::test
