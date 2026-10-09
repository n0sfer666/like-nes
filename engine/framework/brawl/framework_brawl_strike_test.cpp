#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Ring;
namespace graphics = framework::graphics;

fix32 walk_after(bool strike) {
    Ring r;
    r.put(0, 1, 0);
    test::tick(r.pool, r.arena, {test::walk_right()});
    Command c = test::walk_right();
    if (strike) c.strike = test::strike(r.arena, r.arena.jab).strike;
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
    r.play(b, r.arena.jab);
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
    r.play(t, r.arena.flurry);
    r.hit();
    return t.clip == r.arena.flurry && t.hp < 100;
}

void test_no_stun() {
    check(strike_kept(0), "a hit without hitstun leaves the target's strike running");
    check(!strike_kept(8), "control: a hit with hitstun breaks the target's strike");
}

const char* UPPERCUT = "move | uppercut | hit0\n"
                       "type | light\ndamage | 3\ndepth | 4\nhitstop | 0\nhitstun | 0\n"
                       "knock_x | 0\nknock_y | 0\nhits_down | no\nslide | no\n"
                       "move | uppercut | hit1\n"
                       "type | launch\ndamage | 11\ndepth | 4\nhitstop | 0\nhitstun | 0\n"
                       "knock_x | 0\nknock_y | 0\nhits_down | no\nslide | no\n";

std::vector<graphics::ClipSrc> uppercut_clips() {
    std::vector<graphics::ClipSrc> clips = test::dummy_clips();
    graphics::ClipSrc c = test::dummy_clip("uppercut", {false, false, false}, graphics::CLIP_ONCE);
    c.frames[1].boxes.push_back({graphics::BoxKind::Hit, 0, {-60, -24, 4, 4}});
    c.frames[1].boxes.push_back({graphics::BoxKind::Hit, 1, test::REACH});
    clips.push_back(c);
    return clips;
}

struct Uppercut {
    Ring r{uppercut_clips(), std::string(test::DUMMY_TEXT) + UPPERCUT};
    Body* b = &r.put(0, 1, 0);
    Body* t = &r.put(24, -1, 1);

    void start(uint16_t row) {
        Command c;
        c.strike = row;
        test::tick(r.pool, r.arena, {c});
    }
    void finish() {
        for (int k = 0; k < 4; ++k) test::tick(r.pool, r.arena, {});
    }
};

void test_rows_of_one_move() {
    Uppercut u;
    const uint16_t head = u.r.row("uppercut");
    check(u.r.arena.error.empty() && head != NO_STRIKE && u.r.arena.kinds[0].moves[head + 1].head == head,
          "two rows of one name share the first row as their head");
    u.start(head);
    u.finish();
    check(u.t->hp == 100 - 11, "box 1 of a move is judged by its own row");
    Uppercut v;
    v.start(static_cast<uint16_t>(head + 1));
    check(v.b->move == head, "a command naming the second row plays the move from its head");
}

} // namespace

int main() {
    std::printf("brawl: a strike in motion\n");
    test_ground_stop();
    test_jump_clip();
    test_knock_y();
    test_no_stun();
    test_rows_of_one_move();
    return test::verdict("framework-brawl-strike");
}
