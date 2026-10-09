#include <cstdio>

#include "body_hash.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Ring;

const fix32 WALK = fix32::from_int(2);
const fix32 RUN = fix32::from_int(3);

Command stick(int32_t x, uint16_t buttons = 0) {
    Command c;
    c.input.present = true;
    c.input.move.move_x = fix32::from_int(x);
    c.input.buttons = buttons;
    return c;
}

struct Runner {
    Ring r;
    Body* b = nullptr;

    explicit Runner(int32_t height = 0) {
        b = &r.put(0, 1, 0);
        b->pos.y = fix32::from_int(height);
    }
    void step(const Command& c) { test::tick(r.pool, r.arena, {c}); }
    void taps(int32_t x, uint32_t gap) {
        step(stick(x));
        for (uint32_t k = 0; k < gap; ++k) step(stick(0));
        step(stick(x));
    }
};

fix32 speed_after_gap(uint32_t gap) {
    Runner u;
    u.taps(1, gap);
    return u.b->pos.vx;
}

void test_window() {
    const uint32_t tap = Runner().r.arena.kinds[0].run_tap;
    check(tap == 5, "the dummy run window is five ticks");
    check(speed_after_gap(tap - 1) == RUN, "a second tap inside the window starts the run at run_x");
    check(speed_after_gap(tap) == WALK, "a second tap one tick past the window walks");
    Runner u;
    u.step(stick(1));
    check(u.b->pos.vx == WALK && u.b->run.dir == 0, "control: one press walks");
}

void test_held_and_released() {
    Runner u;
    u.taps(1, 1);
    for (int k = 0; k < 10; ++k) u.step(stick(1));
    check(u.b->run.dir == 1 && u.b->pos.vx == RUN, "the run lasts while the direction is held");
    u.step(stick(0));
    u.step(stick(1));
    check(u.b->run.dir == 0 && u.b->pos.vx == WALK, "a release ends the run and the next single press walks");
    Runner v;
    v.taps(1, 1);
    v.step(stick(-1));
    check(v.b->run.dir == 0 && v.b->pos.vx == -WALK && v.b->facing == -1, "a reversal ends the run and walks back");
    Runner w;
    w.step(stick(-1));
    w.step(stick(0));
    w.step(stick(1));
    check(w.b->run.dir == 0 && w.b->pos.vx == WALK, "taps in two directions do not start a run");
    Runner y;
    y.taps(1, 1);
    y.step(stick(0));
    y.step(stick(1));
    check(y.b->run.dir == 0 && y.b->pos.vx == WALK, "the tap that starts a run opens no new window");
    Runner x;
    x.taps(-1, 2);
    check(x.b->run.dir == -1 && x.b->pos.vx == -RUN, "a double tap to the left runs left");
}

void test_air() {
    Runner u(60);
    u.taps(1, 1);
    check(fix32{} < u.b->pos.y && u.b->run.dir == 0 && u.b->pos.vx == WALK, "a double tap in the air does not run");
    Runner v;
    v.taps(1, 1);
    v.step(stick(1, button::JUMP));
    v.step(stick(1));
    check(fix32{} < v.b->pos.y && v.b->run.dir == 1 && v.b->pos.vx == RUN, "a jump from the run keeps run_x in the air");
}

void test_strikes() {
    Runner u;
    u.taps(1, 1);
    Command jab = stick(1);
    jab.strike = u.r.row("jab");
    u.step(jab);
    check(u.b->run.dir == 0 && u.b->pos.vx == fix32{}, "a strike from the run ends it and a plain row stands");
    for (int k = 0; k < 8; ++k) u.step(stick(1));
    check(u.b->move == NO_STRIKE && u.b->pos.vx == WALK, "after the strike the held direction walks");

    Runner v;
    v.taps(1, 1);
    Command run_jab = stick(1);
    run_jab.strike = v.r.row("run_jab");
    const fix32 x0 = v.b->pos.x;
    v.step(run_jab);
    v.step(stick(1));
    check(v.b->clip == v.r.arena.jab && v.b->pos.vx == RUN && v.b->pos.x == x0 + RUN + RUN,
          "a slide row plays its clip and rolls the body with run_x");
    Body& t = v.r.put(32, -1, 1);
    for (int k = 0; k < 3; ++k) v.step(stick(0));
    check(t.hp == 100 - 9, "a slide row on a shared clip hits with its own damage");
    Runner w;
    w.b->facing = -1;
    Command still;
    still.strike = w.r.row("run_jab");
    w.step(still);
    check(w.b->pos.vx == -RUN, "a slide row rolls along the facing, from a standstill too");
}

void test_buffer_edge() {
    Runner u;
    u.r.mid_jab(*u.b);
    Command queued;
    queued.strike = u.r.row("run_jab");
    u.step(queued);
    check(u.b->queued.strike == u.r.row("run_jab"), "a slide row waits in the buffer by its row");
    for (int k = 0; k < 6 && u.b->move != u.r.row("run_jab"); ++k) u.step(Command{});
    check(u.b->move == u.r.row("run_jab") && u.b->run.dir == 0 && u.b->pos.vx == RUN,
          "a buffered slide row starts without a run and still rolls");
}

void test_reaction() {
    Runner u;
    u.taps(1, 1);
    u.b->react = Reaction::Hurt;
    u.b->react_ticks = 3;
    u.step(stick(1));
    check(u.b->run.dir == 0, "a reaction ends the run");
}

uint64_t replayed(bool forget_window) {
    Runner u;
    u.step(stick(1));
    u.step(stick(0));
    BodyPool copy = u.r.pool;
    if (forget_window) copy.bodies[0].run.tap_ticks = 0;
    u.r.pool = copy;
    u.step(stick(1));
    u.step(stick(1));
    return state_hash(u.r.pool);
}

void test_snapshot() {
    Runner u;
    u.taps(1, 1);
    u.step(stick(1));
    check(replayed(false) == state_hash(u.r.pool), "a snapshot taken between the taps replays to the straight-run hash");
    check(replayed(true) != state_hash(u.r.pool), "control: a snapshot without the tap window diverges");
}

} // namespace

int main() {
    std::printf("brawl: the run and the strike from it\n");
    test_window();
    test_held_and_released();
    test_air();
    test_strikes();
    test_buffer_edge();
    test_reaction();
    test_snapshot();
    return test::verdict("framework-brawl-run");
}
