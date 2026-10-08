#include <cstdio>

#include "body_guard.hpp"
#include "body_react.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_guard_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::press;
using test::Ring;
using test::Solo;

const fix32 RUN = fix32::from_int(3);

void test_dodge_rolls_forward() {
    for (const int8_t facing : {int8_t{1}, int8_t{-1}}) {
        Solo u(facing);
        const fix32 speed = facing < 0 ? -RUN : RUN;
        u.step(press(button::DODGE));
        check(u.b->react == Reaction::Dodge && u.b->clip == u.kind().dodge && u.b->elapsed == 0 && u.b->pos.vx == speed,
              "DODGE rolls forward at run_x on its clip");
        for (int k = 0; k < 3; ++k) u.step(press(button::DODGE | button::BLOCK | button::JUMP, -facing));
        check(u.b->react == Reaction::Dodge && u.b->pos.vx == speed && u.b->pos.y == fix32{},
              "the roll is not interrupted or restarted by the stick, block, jump or dodge");
        u.step(press(0));
        check(u.b->react == Reaction::Dodge && u.b->clip == u.kind().dodge && u.b->pos.x == speed * fix32::from_int(5),
              "the roll holds its last tick after dodge ticks of rolling");
        u.step(press(0));
        check(u.b->react == Reaction::None && u.b->pos.vx == fix32{} && u.b->pos.x == speed * fix32::from_int(5),
              "the next tick ends the roll and stops the body");
    }
}

void test_dodge_is_invulnerable() {
    Ring r;
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    check(r.collect().count == 1, "control: the jab reaches a standing target");
    track_guard(t, press(button::DODGE).input, r.arena.kinds[0]);
    bool hit = false;
    for (int k = 0; k < 5; ++k) {
        tick_reaction(t, r.arena.kinds[0]);
        hit = hit || r.collect().count != 0 || t.react != Reaction::Dodge;
    }
    check(!hit, "a rolling body is not hit on any of its dodge ticks");
    track_guard(t, press(0).input, r.arena.kinds[0]);
    check(t.react == Reaction::None && r.collect().count == 1, "the end of the roll makes the body a target again");
}

void test_dodge_needs_a_free_body_on_the_ground() {
    Solo air;
    air.b->pos.y = fix32::from_int(10);
    air.step(press(button::DODGE));
    check(air.b->react == Reaction::None, "no roll in the air");
    Solo busy;
    busy.r.mid_jab(*busy.b);
    busy.step(press(button::DODGE));
    check(busy.b->react == Reaction::None, "no roll in the middle of a strike");
    Solo hurt;
    hurt.b->react = Reaction::Hurt;
    hurt.b->react_ticks = 4;
    hurt.step(press(button::DODGE));
    check(hurt.b->react == Reaction::Hurt, "no roll in a reaction");
    Solo guard;
    guard.step(press(button::BLOCK));
    guard.step(press(button::BLOCK | button::DODGE));
    check(guard.b->react == Reaction::Dodge, "a roll starts from the block");
}

void test_archetype_needs_the_dodge_clip() {
    check(test::arena_without("none").empty(), "control: the full sheet makes an archetype");
    check(test::arena_without("dodge").starts_with("no clip 'dummy/dodge' in the clips"),
          "an archetype without the dodge clip is refused with its name");
}

} // namespace

int main() {
    std::printf("brawl: dodge\n");
    test_dodge_rolls_forward();
    test_dodge_is_invulnerable();
    test_dodge_needs_a_free_body_on_the_ground();
    test_archetype_needs_the_dodge_clip();
    return test::verdict("framework-brawl-dodge");
}
