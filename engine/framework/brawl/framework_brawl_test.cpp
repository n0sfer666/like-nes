#include "body_pool.hpp"
#include "depth_step.hpp"
#include "framework_brawl_check.hpp"

namespace {

using namespace framework::brawl;
using test::check;

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }

const DepthProfile PROFILE{px(2), px(1), fix32::from_raw(fix32::ONE / 2), px(6)};

DepthFloor open_floor() {
    DepthFloor f;
    f.band = FloorRect{px(-1000), px(-1000), px(1000), px(1000)};
    return f;
}

BrawlInput held(fix32 mx, fix32 mz, uint16_t buttons = 0) {
    BrawlInput in;
    in.move.move_x = mx;
    in.move.move_z = mz;
    in.buttons = buttons;
    in.present = true;
    return in;
}

void test_seq_is_monotonic_and_never_reused() {
    BodyPool pool;
    const EntId a = pool.spawn(Body{});
    const EntId b = pool.spawn(Body{});
    const EntId c = pool.spawn(Body{});
    check(a.seq == 1 && b.seq == 2 && c.seq == 3, "seq starts at 1 and grows by one");
    check(pool.despawn(b), "a living body despawns");
    check(!pool.despawn(b), "a dead one does not despawn twice");
    const EntId d = pool.spawn(Body{});
    check(d.seq == 4, "the freed slot does not hand its seq out again");
    check(pool.count == 3 && pool.bodies[0].id == a && pool.bodies[1].id == c &&
              pool.bodies[2].id == d,
          "the pool stays ordered by seq");
    check(pool.find(b) == nullptr && pool.find(c) != nullptr, "find sees only the living");
}

void test_a_full_pool_refuses() {
    BodyPool pool;
    for (uint32_t i = 0; i < POOL_CAPACITY; ++i) pool.spawn(Body{});
    const EntId over = pool.spawn(Body{});
    check(over.seq == 0, "a spawn over capacity returns no id");
    check(pool.count == POOL_CAPACITY && pool.next_seq == POOL_CAPACITY + 1,
          "and spends no seq");
}

void test_input_drives_and_absence_stops() {
    const DepthFloor f = open_floor();
    Body b;
    step_body(b, held(px(1), px(-1)), PROFILE, f);
    check(b.pos.x == px(2) && b.pos.z == px(-1), "the stick moves the body by the profile speeds");
    step_body(b, held(px(-1), fix32{}), PROFILE, f);
    check(b.facing == -1, "moving left turns the body left");
    step_body(b, held(fix32{}, fix32{}), PROFILE, f);
    check(b.facing == -1, "standing still keeps the facing");
    step_body(b, held(px(5), fix32{}), PROFILE, f);
    check(b.pos.x == px(2), "a stick past one is clamped to the profile speed");
    const fix32 x = b.pos.x;
    step_body(b, BrawlInput{held(px(1), px(1)).move, 0, false}, PROFILE, f);
    check(b.pos.x == x && b.pos.vx == fix32{}, "an absent player does not move");
    check(b.age == 5, "every step ages the body by one tick");
}

void test_the_jump_lands_on_the_floor() {
    const DepthFloor f = open_floor();
    Body b;
    step_body(b, held(fix32{}, fix32{}, button::JUMP), PROFILE, f);
    check(fix32{} < b.pos.y, "the jump leaves the floor on its own tick");
    uint32_t air = 1;
    while (fix32{} < b.pos.y && air < 100) {
        step_body(b, held(fix32{}, fix32{}, button::JUMP), PROFILE, f);
        ++air;
    }
    check(b.pos.y == fix32{} && b.pos.vy == fix32{}, "it lands exactly on y = 0 at rest");
    check(air == 23, "the arc of jump 6 under gravity 1/2 is 23 ticks");
}

} // namespace

int main() {
    std::printf("brawl: depth bodies, the pool and the step\n");
    test_seq_is_monotonic_and_never_reused();
    test_a_full_pool_refuses();
    test_input_drives_and_absence_stops();
    test_the_jump_lands_on_the_floor();
    return test::verdict("framework-brawl");
}
