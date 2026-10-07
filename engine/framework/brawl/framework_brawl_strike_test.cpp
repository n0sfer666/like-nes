#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Ring;

fix32 walk_after(bool strike) {
    Ring r;
    r.put(0, 1, 0);
    test::tick(r.pool, r.arena, {test::walk_right()});
    Command c = test::walk_right();
    if (strike) c.strike = r.arena.jab;
    test::tick(r.pool, r.arena, {c});
    return r.pool.bodies[0].pos.vx;
}

void test_ground_stop() {
    check(walk_after(true) == fix32{}, "a strike on the ground stops the walk");
    check(!(walk_after(false) == fix32{}), "control: without a strike the walk goes on");
}

void test_jump_clip() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    b.clip = r.arena.jab;
    b.elapsed = 3;
    b.pos.y = fix32::from_int(20);
    b.pos.vy = fix32::from_int(4);
    test::tick(r.pool, r.arena, {});
    check(b.clip == r.arena.kinds[0].jump && b.elapsed == 4,
          "a strike that ends in the air enters the jump clip at the ticks already flown");
}

fix32 knock_down(int32_t target_y) {
    Ring r;
    r.move(r.arena.jab).knock_y = fix32::from_int(-2);
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    t.pos.y = fix32::from_int(target_y);
    return r.hit().count == 1 ? t.pos.vy : fix32::from_int(99);
}

void test_knock_y() {
    check(knock_down(0) == fix32{}, "a downward knock leaves a target on the ground at rest");
    check(knock_down(10) == fix32::from_int(-2), "a downward knock drives an airborne target down");
}

bool strike_kept(uint32_t hitstun) {
    Ring r;
    r.move(r.arena.jab).hitstun = hitstun;
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    t.clip = r.arena.flurry;
    r.hit();
    return t.clip == r.arena.flurry && t.hp < 100;
}

void test_no_stun() {
    check(strike_kept(0), "a hit without hitstun leaves the target's strike running");
    check(!strike_kept(8), "control: a hit with hitstun breaks the target's strike");
}

} // namespace

int main() {
    std::printf("brawl: a strike in motion\n");
    test_ground_stop();
    test_jump_clip();
    test_knock_y();
    test_no_stun();
    return test::verdict("framework-brawl-strike");
}
