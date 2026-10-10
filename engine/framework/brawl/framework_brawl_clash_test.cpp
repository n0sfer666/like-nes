#include <algorithm>
#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_grab_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Grapple;

struct Double {
    bool thrown = false;
    bool struck = false;
    bool kept = false;
    uint16_t hitstop = 0;
};

Double double_grab(bool left_first, bool both) {
    Grapple g;
    if (!left_first && both) g.reaching(32, -1, 0);
    g.reaching(0, 1, 0);
    if (left_first && both) g.reaching(32, -1, 0);
    Body& b = g.r.put(16, -1, 1);
    const HitEvents e = g.hit();
    Double out{b.react == Reaction::Thrown, false, false, 0};
    for (uint32_t i = 0; i < e.count; ++i) out.kept |= e.at[i].kept;
    for (uint32_t i = 0; i < g.r.pool.count; ++i) {
        out.struck |= g.r.pool.bodies[i].struck.count > 0;
        if (!(g.r.pool.bodies[i].id == b.id)) out.hitstop = std::max(out.hitstop, g.r.pool.bodies[i].hitstop);
    }
    return out;
}

void test_double_grab_tears_both() {
    check(double_grab(true, false).thrown && double_grab(true, false).kept, "control: one grab throws the target");
    for (const bool left_first : {true, false}) {
        const Double d = double_grab(left_first, true);
        check(!d.thrown && !d.struck && !d.kept && d.hitstop == 0,
              "two grabs of one target in one tick tear both, without contact, hitstop or a kept event, in any seq order");
    }
}

void test_mutual_grab_tears_both() {
    for (const bool left_first : {true, false}) {
        Grapple g;
        Body& a = left_first ? g.reaching(0, 1, 0) : g.reaching(16, -1, 1);
        Body& b = left_first ? g.reaching(16, -1, 1) : g.reaching(0, 1, 0);
        check(g.hit().count == 2, "control: both grabs reach");
        check(a.react == Reaction::None && b.react == Reaction::None && a.hp == 100 && b.hp == 100,
              "two bodies grabbing each other in one tick both miss");
    }
}

void test_struck_grabber_loses_the_grab() {
    for (const int32_t striker : {-16, -80}) {
        Grapple g;
        Body& a = g.reaching(0, 1, 0);
        Body& b = g.r.put(16, -1, 1);
        g.r.mid_jab(g.r.put(striker, 1, 1));
        g.hit();
        const bool near = striker == -16;
        check((b.react == Reaction::Thrown) != near && (a.react == Reaction::Hurt) == near,
              "a grabber struck in the same tick is hurt and loses the grab; a miss leaves the throw");
    }
}

void test_strike_and_grab_add_up() {
    Grapple g;
    g.reaching(0, 1, 0);
    Body& b = g.r.put(16, -1, 1);
    g.r.mid_jab(g.r.put(32, -1, 0));
    check(g.hit().count == 2, "control: the jab and the grab both reach the target");
    check(b.react == Reaction::Thrown && b.hp == 100 - 6 - 5, "a strike and a grab in one tick add up and throw");
}

Reaction led_by(HitType second, uint32_t hitstun) {
    test::Ring r;
    r.move(r.arena.flurry).type = second;
    r.move(r.arena.flurry).hitstun = hitstun;
    r.move(r.arena.jab).hitstun = 20;
    r.mid_jab(r.put(0, 1, 0));
    Body& f = r.put(48, -1, 0);
    r.play(f, r.arena.flurry);
    Body& t = r.put(24, -1, 1);
    r.hit();
    return t.react;
}

void test_launch_leads_over_hitstun() {
    check(led_by(HitType::Launch, 0) == Reaction::Fall, "a launch leads a heavier hitstun in the same tick");
    check(led_by(HitType::Heavy, 0) == Reaction::Hurt, "control: without the launch the longer hitstun leads");
}

} // namespace

int main() {
    std::printf("brawl: grabs and strikes in one tick\n");
    test_double_grab_tears_both();
    test_mutual_grab_tears_both();
    test_struck_grabber_loses_the_grab();
    test_strike_and_grab_add_up();
    test_launch_leads_over_hitstun();
    return test::verdict("framework-brawl-clash");
}
