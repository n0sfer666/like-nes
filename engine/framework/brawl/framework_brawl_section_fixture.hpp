#pragma once
#include <cstdint>
#include <vector>

#include "framework_brawl_stage_fixture.hpp"
#include "grid.hpp"
#include "platform_body.hpp"
#include "platform_world.hpp"
#include "shape.hpp"
#include "stage_sections.hpp"

namespace framework::brawl::test {

constexpr int32_t FLOOR = 240;
constexpr int32_t PLATFORM_X0 = 400;
constexpr int32_t PLATFORM_X1 = 800;
constexpr int32_t TILE = 4;
constexpr int32_t GRID_TOP = 160;
constexpr int32_t HULL_HALF_W = 8;

struct Solid {
    int32_t x0, y0, x1, y1;
};

inline uint32_t cell(int32_t v, int32_t origin) { return static_cast<uint32_t>((v - origin) / TILE); }

// Уровень: belt [0, 400), platform [400, 800), belt [800, 1600). Пол platform — тайлы с верхом на
// линии belt `FLOOR`, поверх — то, что положил случай (уступ, стена, потолок) в пикселях, кратных
// `TILE`. Пол belt — полоса `z` [200, 300].
struct Sections {
    Stage s;
    tilemap::TileGrid grid{{fix32{}, at(GRID_TOP)}, at(TILE), 400, 40};
    PlatformWorld platform;

    explicit Sections(const std::vector<Solid>& solids = {}) {
        s.arena.floor.band = FloorRect{at(-1000), at(200), at(2000), at(300)};
        s.level.sections.push_back({area(0, 0, PLATFORM_X0, 400), scene::SectionMode::Belt});
        s.level.sections.push_back({area(PLATFORM_X0, 0, PLATFORM_X1, 400), scene::SectionMode::Platform});
        s.level.sections.push_back({area(PLATFORM_X1, 0, 1600, 400), scene::SectionMode::Belt});
        grid.fill(0, cell(FLOOR, GRID_TOP), 400, 40, tilemap::TILE_SOLID);
        for (const Solid& r : solids)
            grid.fill(cell(r.x0, 0), cell(r.y0, GRID_TOP), cell(r.x1, 0), cell(r.y1, GRID_TOP), tilemap::TILE_SOLID);
        platform.scene = character::CollisionScene{nullptr, &grid};
        platform.hull.shape = physics::sanitize(physics::box(at(HULL_HALF_W), at(16)));
        platform.profile = character::default_profile();
        platform.dt = at(1) / at(60);
        platform.derived = character::derive(platform.profile, platform.dt);
        platform.dead_y = at(24);
    }

    BrawlWorld world() const {
        BrawlWorld w = s.world();
        w.platform = &platform;
        return w;
    }

    EntId body(int32_t x, int32_t z = FLOOR, uint8_t team = 0) {
        return s.pool.spawn(dummy(x, z, 1, team, s.arena.kinds[0]));
    }

    Body& get(EntId id) { return *s.pool.find(id); }

    void step(const std::vector<Command>& c) {
        HitEvents events;
        step_stage(s.pool, c, world(), s.camera, s.seats, s.sections, events);
    }

    bool platform_mode(EntId id) const { return s.sections.find(id) != nullptr; }
};

inline Command go(int32_t dir, bool jump = false) {
    Command c;
    c.input = walk(dir);
    if (jump) c.input.buttons = button::JUMP;
    return c;
}

inline fix32 before(int32_t v) { return fix32::from_raw(at(v).raw - 1); }

// Контроллер держит опору с зазором до `SKIN` и скорость в px/s: зеркало в `DepthBody` совпадает с
// целыми пикселями с точностью до долей пикселя, а не до кванта.
inline bool about(fix32 v, int32_t px) { return abs_fix(v - at(px)) < at(1) / at(8); }

} // namespace framework::brawl::test
