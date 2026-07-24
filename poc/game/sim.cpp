#include "sim.hpp"
#include "combat.hpp"

namespace game {
namespace {

const fix32 SHIP_SPEED = fix32::from_int(340);
const fix32 X_LIMIT = fix32::from_int(HALF_W - 56);
const fix32 Y_LIMIT = fix32::from_int(HALF_H - 40);
const fix32 WRAP_EDGE = fix32::from_int(HALF_W + 40);
const fix32 WRAP_SPAN = fix32::from_int(VIEW_W + 80);

fix32 clamp(fix32 v, fix32 lo, fix32 hi) {
    if (v < lo) return lo;
    if (hi < v) return hi;
    return v;
}

uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }

} // namespace

void spawn(flecs::world& world, GameState& gs) {
    flecs::entity ship = world.entity();
    ship.set<Transform>({fix32::from_int(-300), fix32{}});
    ship.set<Velocity>({fix32{}, fix32{}});
    ship.add<Ship>();
    ship.set<EntId>({gs.seq++});

    uint32_t s = 0x9e3779b9u;
    for (int i = 0; i < STAR_COUNT; ++i) {
        const int layer = i % 3;
        const int px = (int)(lcg(s) % (uint32_t)(VIEW_W + 80)) - (HALF_W + 40);
        const int py = (int)(lcg(s) % (uint32_t)VIEW_H) - HALF_H;
        const fix32 speed = fix32::from_int(40 + layer * 44);
        const fix32 size = fix32::from_int(2 + layer * 2);
        const uint8_t shade = (uint8_t)(110 + layer * 60);
        world.entity().set<Transform>({fix32::from_int(px), fix32::from_int(py)})
                      .set<Star>({speed, size, shade});
    }
}

void step(flecs::world& world, GameState& gs, const input::InputFrame& in, fix32 dt) {
    if (gs.phase == PH_Play || gs.phase == PH_Boss) {  // корабль двигается только в бою
        world.each([&](flecs::entity e, Transform& t, Velocity& v) {
            if (!e.has<Ship>()) return;
            v.x = in.axes[AX_MoveX] * SHIP_SPEED;
            v.y = in.axes[AX_MoveY] * SHIP_SPEED;
            t.x = clamp(t.x + v.x * dt, -X_LIMIT, X_LIMIT);
            t.y = clamp(t.y + v.y * dt, -Y_LIMIT, Y_LIMIT);
        });
    }

    world.each([&](Transform& t, Star& star) {
        t.x = t.x - star.speed * dt;
        if (t.x < -WRAP_EDGE) t.x = t.x + WRAP_SPAN;
    });

    combat_step(world, gs, in, dt);
}

} // namespace game
