#include <cstdio>

#include "body_react.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_grab_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Grapple;

constexpr uint32_t STEPS = 60;

struct Flight {
    Grapple g;
    Body* a = nullptr;
    Body* b = nullptr;

    explicit Flight(int32_t knock_x = 3, int8_t way = 1) {
        g.r.arena.kinds[0].moves[g.r.row("grab")].strike.knock_x = fix32::from_int(knock_x);
        a = &g.reaching(0, way, 0);
        b = &g.r.put(16 * way, static_cast<int8_t>(-way), 1);
        g.hit();
    }
    void step(std::vector<Command> c = {}, bool friendly_fire = false) {
        test::tick(g.r.pool, g.r.arena, std::move(c), friendly_fire);
    }
};

void test_thrown_body_strikes_as_a_projectile() {
    for (const int32_t way : {1, -1}) {
        Flight f(3 * way);
        Body& mate = f.g.r.put(4 * way, -1, 0);
        Body& d = f.g.r.put(40 * way, static_cast<int8_t>(-way), 1);
        bool hit = false;
        for (uint32_t k = 0; k < STEPS && f.b->react == Reaction::Thrown && !hit; ++k) {
            f.step();
            hit = d.hp < 100;
        }
        check(hit && d.hp == 100 - 8 && d.react == Reaction::Fall && d.pos.vx == fix32::from_int(2 * way),
              "the thrown body hits on the throw row of its thrower, knocking along its flight either way");
        check(f.a->hp == 100 && mate.hp == 100, "the thrown body spares its thrower and the thrower's team");
    }
}

void test_straight_up_throw_knocks_the_way_the_thrower_faces() {
    for (const int8_t way : {int8_t{1}, int8_t{-1}}) {
        Flight f(0, way);
        Body& d = f.g.r.put(0, way, 1);
        for (uint32_t k = 0; k < STEPS && f.b->react == Reaction::Thrown && d.hp == 100; ++k) f.step();
        check(d.hp < 100 && d.pos.vx == fix32::from_int(2 * way),
              "a thrown body without speed across knocks the way its thrower faces");
    }
}

void test_thrown_body_spares_its_thrower_under_friendly_fire() {
    Flight f;
    for (uint32_t k = 0; k < STEPS && f.b->react == Reaction::Thrown; ++k) f.step({}, true);
    check(f.b->react == Reaction::Down && f.a->hp == 100, "the thrown body never hits its own thrower");
}

void test_thrown_body_lands_and_gets_up() {
    Flight f;
    const Archetype& k = f.g.kind();
    bool open = false;
    uint32_t flight = 0;
    while (flight < STEPS && f.b->react == Reaction::Thrown) {
        open |= vulnerable(*f.b);
        f.step();
        ++flight;
    }
    check(!open && flight > 2, "a thrown body cannot be hit while it flies");
    check(f.b->react == Reaction::Down && f.b->clip == k.down && f.b->pos.y == fix32{} && f.b->pos.vx == fix32{},
          "a thrown body lands lying down at rest");
    check(f.b->grip.by == EntId{} && f.b->grip.team == 0, "landing lets go of the grip");
    while (flight < 2 * STEPS && f.b->react != Reaction::None) {
        f.step();
        ++flight;
    }
    check(f.b->react == Reaction::None && f.b->clip == k.idle, "the thrown body gets up and stands");
}

void test_thrower_is_busy_to_the_end_of_the_throw() {
    Flight f;
    const Command jab = test::strike(f.g.r.arena, f.g.r.arena.jab);
    uint32_t busy = 0;
    while (busy < STEPS && f.a->clip == f.g.kind().throws) {
        f.step({jab});
        ++busy;
    }
    check(busy == 2 + 4, "the thrower plays the throw clip through its hitstop and every tick of the clip");
    f.step();
    check(f.a->clip == f.g.r.arena.jab, "a strike pressed during the throw waits in the buffer and starts after it");
}

void test_throw_row_is_not_a_command() {
    Grapple g;
    Body& a = g.r.put(0, 1, 0);
    Command c;
    c.strike = g.r.row("throw");
    test::tick(g.r.pool, g.r.arena, {c});
    check(a.queued.ticks == 0 && a.clip == g.kind().idle, "the throw row cannot be struck by a command");
    c.strike = g.r.row("grab");
    test::tick(g.r.pool, g.r.arena, {c});
    check(a.clip == g.reach, "control: the grab row is struck by a command");
}

uint32_t finishes(bool hits_down, Reaction react, Body& out) {
    test::Ring r;
    r.move(r.arena.jab).hits_down = hits_down;
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    t.react = react;
    t.react_ticks = 5;
    const uint32_t n = r.hit().count;
    out = t;
    return n;
}

void test_no_strike_reaches_a_thrown_body() {
    Body t;
    check(finishes(false, Reaction::Thrown, t) == 0 && finishes(true, Reaction::Thrown, t) == 0,
          "no strike reaches a thrown body in flight");
}

void test_hits_down_finishes_a_lying_body() {
    Body t;
    check(finishes(false, Reaction::Down, t) == 0, "control: an ordinary strike does not reach a lying body");
    check(finishes(true, Reaction::Down, t) == 1, "a hits_down strike reaches a lying body");
    check(t.hp == 95 && t.hitstop == 3 && t.react == Reaction::Down && t.react_ticks == 5 && t.pos.vx == fix32{},
          "it deals damage and hitstop without knock, the down timer untouched");
    check(finishes(true, Reaction::Fall, t) == 0 && finishes(true, Reaction::Getup, t) == 0,
          "a hits_down strike does not reach a falling or rising body");
    check(finishes(true, Reaction::None, t) == 1 && t.react == Reaction::Hurt, "control: it hits a standing body as usual");
}

} // namespace

int main() {
    std::printf("brawl: the thrown body and the finish on the ground\n");
    test_thrown_body_strikes_as_a_projectile();
    test_straight_up_throw_knocks_the_way_the_thrower_faces();
    test_thrown_body_spares_its_thrower_under_friendly_fire();
    test_thrown_body_lands_and_gets_up();
    test_thrower_is_busy_to_the_end_of_the_throw();
    test_throw_row_is_not_a_command();
    test_no_strike_reaches_a_thrown_body();
    test_hits_down_finishes_a_lying_body();
    return test::verdict("framework-brawl-throw");
}
