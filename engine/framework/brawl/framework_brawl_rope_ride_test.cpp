#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_rope_fixture.hpp"
#include "rope_ride.hpp"

// Седок троса (спека #25, В8в): отпускание прыжком с касательной скоростью, остывание, сброс
// ударом, стоп удара без сброса, упор в стену, раскачка до предела.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;

EntId ride(Swing& s, int32_t degrees) {
    const EntId id = s.fighter(500);
    s.ropes.at[0].arc = deflection(at(LENGTH), degrees);
    s.hang_hands(id, s.end());
    s.step({grab()});
    return id;
}

void release() {
    Swing s;
    const EntId id = ride(s, 30);
    s.idle(10);
    const RopeState before = s.ropes.at[0];
    const fix32 dt = s.w.platform.dt;
    const std::vector<Command> c{leap()};
    ride_rope(s.ropes.at[0], s.w.s.level.ropes[0], s.w.s.pool, c, s.w.s.sections, s.w.world());
    const framework::character::Character& h = s.section(id).c;
    const Vec2 v = rope_tangent(before.arc, at(LENGTH)) * (before.speed / dt);
    check(h.velocity.x == v.x && h.velocity.y == v.y && h.jump_was_held, "the rider leaves without the rope velocity");
    check(s.ropes.at[0].rider == EntId{} && s.ropes.at[0].cooldown == ROPE_COOLDOWN, "the jump does not free the rope");
    Swing full;
    const EntId leaper = ride(full, 30);
    full.idle(10);
    full.step({leap()});
    check(full.ropes.at[0].rider == EntId{} && !full.section(leaper).c.jump_active,
          "the release jumps off the rope as from the ground or retakes it");
    Swing held;
    const EntId keeps = held.fighter(500);
    held.hang_hands(keeps, held.end());
    held.step({leap()});
    held.step({leap()});
    check(held.ropes.at[0].rider == keeps, "a jump held through the grab drops the rider");
}

// Захват сразу после схода с лифта, с лестницы или в прыжке: контроллер держит опору в окне
// прощения, буфер и прыжок. На тросе контроллер стоит — со троса уходят чистым падением.
void fresh_air() {
    Swing s;
    const EntId id = s.fighter(500);
    s.hang_hands(id, s.end());
    framework::character::Character& c = s.section(id).c;
    c.state = framework::character::MoveState::Ladder;
    c.coyote_left = 5;
    c.buffer_left = 5;
    c.jump_active = true;
    c.support = framework::physics::BodyId{0};
    s.step({grab()});
    check(s.ropes.at[0].rider == id && c.state == framework::character::MoveState::Air && c.coyote_left == 0 &&
              c.buffer_left == 0 && !c.jump_active && !c.support.valid(),
          "the rider keeps a ladder, a coyote window, a buffered jump, a jump or a support from before the grab");
}

void cooldown() {
    Swing s;
    const EntId id = ride(s, 0);
    s.step({leap()});
    uint32_t taken = 0;
    for (uint32_t t = 1; t <= ROPE_COOLDOWN && taken == 0; ++t) {
        s.hang_hands(id, s.end());
        s.step({grab()});
        if (s.ropes.at[0].rider == id) taken = t;
    }
    check(taken == ROPE_COOLDOWN, "the rope is retaken before or after its cooldown");
}

void drops() {
    Swing hit;
    const EntId id = ride(hit, 0);
    Body& b = hit.w.get(id);
    b.react = Reaction::Hurt;
    b.react_ticks = 10;
    b.pos.vx = at(3);
    const fix32 x = b.pos.x;
    hit.step({});
    check(hit.ropes.at[0].rider == EntId{} && hit.ropes.at[0].cooldown == ROPE_COOLDOWN,
          "a hit does not drop the rider");
    check(at(2) < hit.w.get(id).pos.x - x, "the dropped rider loses the knockback");
    Swing dead;
    dead.w.get(ride(dead, 0)).hp = 0;
    dead.step({});
    Swing gone;
    gone.w.s.pool.despawn(ride(gone, 0));
    gone.step({});
    Swing gripped;
    gripped.w.get(ride(gripped, 0)).grip.by = EntId{99};
    gripped.step({});
    check(dead.ropes.at[0].rider == EntId{} && gone.ropes.at[0].rider == EntId{} &&
              gripped.ropes.at[0].rider == EntId{},
          "a dead, removed or gripped rider stays on the rope");
}

void hitstop() {
    Swing s;
    const EntId id = ride(s, 30);
    s.idle(5);
    s.w.get(id).hitstop = 3;
    const RopeState before = s.ropes.at[0];
    s.idle(3);
    const RopeState frozen = s.ropes.at[0];
    check(frozen.rider == id && frozen.arc == before.arc && frozen.speed == before.speed,
          "a rider in hitstop falls off or the rope swings without them");
    s.idle(1);
    check(s.ropes.at[0].rider == id && !(s.ropes.at[0].arc == before.arc), "the rope stays frozen after the hitstop");
}

void wall() {
    Swing s({{640, 160, 660, 236}});
    const EntId id = ride(s, -45);
    const fix32 half = hull_half(s.w.platform).x;
    fix32 reached = at(0);
    bool stopped = false;
    for (uint32_t t = 0; t < 60; ++t) {
        const fix32 arc = s.ropes.at[0].arc;
        s.step({});
        reached = framework::max_fix(reached, s.hands(id).x + half);
        stopped = stopped || (s.ropes.at[0].speed == fix32{} && s.ropes.at[0].arc == arc);
    }
    check(stopped && !(at(640) < reached) && at(634) < reached, "the wall does not stop the rope at the rider");
}

void pump() {
    Swing s;
    const EntId id = ride(s, 0);
    const fix32 cap = rope_cap(at(LENGTH));
    bool capped = false;
    bool over = false;
    for (uint32_t t = 0; t < 3000 && !capped; ++t) {
        s.step({go(s.ropes.at[0].speed < fix32{} ? -1 : 1)});
        capped = framework::abs_fix(s.ropes.at[0].arc) == cap && s.ropes.at[0].speed == fix32{};
        over = over || cap < framework::abs_fix(s.ropes.at[0].arc);
    }
    check(capped && !over && s.ropes.at[0].rider == id, "pumping does not reach the horizontal or passes it");
    s.step({go(-1)});
    const int32_t left = s.w.get(id).facing;
    s.step({go(1)});
    check(left == -1 && s.w.get(id).facing == 1, "pumping does not turn the rider");
}

} // namespace

int main() {
    release();
    fresh_air();
    cooldown();
    drops();
    hitstop();
    wall();
    pump();
    return verdict("framework-brawl-rope-ride");
}
