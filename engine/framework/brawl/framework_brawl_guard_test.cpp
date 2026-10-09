#include <cstdio>

#include "body_clip.hpp"
#include "body_guard.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_guard_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::press;
using test::Ring;
using test::Solo;

const fix32 HALF = fix32::from_int(1) / fix32::from_int(2);

struct Duel {
    Ring r;
    Body* attacker = nullptr;
    Body* target = nullptr;

    explicit Duel(int8_t target_facing = -1, HitType type = HitType::Light) {
        Strike& s = r.move(r.arena.jab);
        s.type = type;
        s.knock_y = type == HitType::Launch ? fix32::from_int(3) : fix32{};
        attacker = &r.put(0, 1, 0);
        r.mid_jab(*attacker);
        target = &r.put(24, target_facing, 1);
        track_guard(*target, press(button::BLOCK).input, r.arena.kinds[0]);
    }
    void hold(uint32_t ticks) {
        for (uint32_t k = 0; k < ticks; ++k) test::tick(r.pool, r.arena, {Command{}, press(button::BLOCK)});
    }
};

void test_block_holds_and_releases() {
    Solo u;
    u.step(press(0, 1));
    u.step(press(button::BLOCK, 1));
    check(u.b->react == Reaction::Block && u.b->clip == u.kind().block && u.b->elapsed == 0,
          "BLOCK held starts the block on its first frame");
    for (int k = 0; k < 10; ++k) u.step(press(button::BLOCK, 1));
    check(u.b->react == Reaction::Block && u.b->pos.x == fix32::from_int(2) && u.b->pos.vx == fix32{},
          "the block stops a walking body and the stick does not walk");
    u.step(press(0, 1));
    check(u.b->react == Reaction::None && u.b->clip != u.kind().block && u.b->pos.vx == fix32::from_int(2),
          "a release ends the block on the same tick and the stick walks");
}

void test_block_needs_a_free_body_on_the_ground() {
    Solo air;
    air.b->pos.y = fix32::from_int(10);
    air.step(press(button::BLOCK));
    check(air.b->react == Reaction::None, "no block in the air");
    Solo busy;
    busy.r.mid_jab(*busy.b);
    busy.step(press(button::BLOCK));
    check(busy.b->react == Reaction::None && striking(*busy.b, busy.kind()), "no block in the middle of a strike");
    Solo hurt;
    hurt.b->react = Reaction::Hurt;
    hurt.b->react_ticks = 4;
    hurt.step(press(button::BLOCK));
    check(hurt.b->react == Reaction::Hurt, "no block in a reaction");
}

void test_strike_waits_for_the_release() {
    Solo u;
    u.step(press(button::BLOCK));
    Command jab = test::strike(u.r.arena, u.r.arena.jab);
    jab.input = press(button::BLOCK).input;
    u.step(jab);
    check(u.b->react == Reaction::Block && !striking(*u.b, u.kind()), "a strike pressed in the block does not start");
    u.step(press(0));
    check(u.b->react == Reaction::None && u.b->move == jab.strike, "the release starts the buffered strike");
}

void test_front_light_and_heavy_are_blocked() {
    for (const HitType type : {HitType::Light, HitType::Heavy}) {
        Duel d(-1, type);
        check(d.r.hit().count == 1, "control: the strike reaches the blocking target");
        check(d.target->hp == 100 && d.target->react == Reaction::Block, "a strike from the front deals nothing");
        check(d.target->hitstop == 3 && d.attacker->hitstop == 3, "both bodies stop for the strike's hitstop");
        check(d.target->pos.vx == HALF && d.target->react_ticks == 8, "the block slides at half knock_x for hitstun");
        d.hold(3 + 7);
        check(d.target->pos.vx == HALF, "the slide lasts through hitstop and hitstun");
        d.hold(1);
        check(d.target->pos.vx == fix32{} && d.target->pos.x == fix32::from_int(28) &&
                  d.target->react == Reaction::Block,
              "the slide stops after hitstun ticks and the block holds");
    }
}

void test_release_ends_the_slide() {
    Duel d;
    d.hold(3 + 2);
    check(d.target->pos.vx == HALF, "control: the block still slides");
    test::tick(d.r.pool, d.r.arena, {Command{}, press(0)});
    check(d.target->react == Reaction::None && d.target->pos.vx == fix32{},
          "a release in the slide leaves the block and stops the body");
}

void test_front_is_judged_by_position() {
    Ring r;
    Strike& s = r.move(r.arena.jab);
    s.type = HitType::Light;
    Body& t = r.put(24, -1, 1);
    t.react = Reaction::Block;
    const Body& level = r.put(24, 1, 0);
    const Body& back_turned = r.put(32, 1, 0);
    const Body& facing_away = r.put(16, -1, 0);
    check(guards(t, level, s), "an attacker on the same x is in front of a left-facing block");
    t.facing = 1;
    check(guards(t, level, s), "an attacker on the same x is in front of a right-facing block");
    check(guards(t, back_turned, s), "the front is the position, not the attacker's facing");
    check(!guards(t, facing_away, s), "an attacker behind is behind whichever way he faces");
}

void test_back_and_launch_break_the_block() {
    Duel back(1);
    back.r.hit();
    check(back.target->hp == 95 && back.target->react == Reaction::Hurt, "a strike from behind hits the block");
    Duel launch(-1, HitType::Launch);
    launch.r.hit();
    check(launch.target->hp == 95 && launch.target->react == Reaction::Fall, "a launch from the front hits the block");
    Duel tap(1);
    tap.r.move(tap.r.arena.jab).hitstun = 0;
    tap.r.hit();
    check(tap.target->hp == 95 && tap.target->react == Reaction::Block && tap.target->pos.vx == fix32{},
          "a strike without hitstun from behind hurts the block and does not push it");
    Duel lift(1);
    Strike& up = lift.r.move(lift.r.arena.jab);
    up.hitstun = 0;
    up.knock_y = fix32::from_int(2);
    lift.r.hit();
    check(lift.target->hp == 95 && lift.target->react == Reaction::None && lift.target->pos.vy == fix32::from_int(2),
          "a strike that lifts the block off the ground ends it");
}

void test_mixed_tick() {
    Duel d;
    Body& behind = d.r.put(48, -1, 0);
    d.r.mid_jab(behind);
    check(d.r.hit().count == 2, "control: both strikes reach the target");
    check(d.target->hp == 95 && d.target->react == Reaction::Hurt && d.target->pos.vx == -fix32::from_int(1),
          "a strike from behind in the same tick hits and the front one adds nothing");
}

void test_block_is_a_contact_for_the_chain() {
    Duel d;
    d.attacker->chain = 0;
    d.r.hit();
    check(d.attacker->struck.count == 1, "a blocked strike is remembered by the attacker");
    Command next = test::strike(d.r.arena, d.r.arena.jab);
    test::tick(d.r.pool, d.r.arena, {next, press(button::BLOCK)});
    d.hold(4);
    check(d.attacker->chain == 1, "the chain steps on a blocked strike");
}

void test_archetype_needs_the_block_clip() {
    check(test::arena_without("none").empty(), "control: the full sheet makes an archetype");
    check(test::arena_without("block").starts_with("no clip 'dummy/block' in the clips"),
          "an archetype without the block clip is refused with its name");
}

} // namespace

int main() {
    std::printf("brawl: block\n");
    test_block_holds_and_releases();
    test_block_needs_a_free_body_on_the_ground();
    test_strike_waits_for_the_release();
    test_front_light_and_heavy_are_blocked();
    test_release_ends_the_slide();
    test_front_is_judged_by_position();
    test_back_and_launch_break_the_block();
    test_mixed_tick();
    test_block_is_a_contact_for_the_chain();
    test_archetype_needs_the_block_clip();
    return test::verdict("framework-brawl-guard");
}
