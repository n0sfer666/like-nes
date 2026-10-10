#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"
#include "hit_apply.hpp"
#include "hit_collect.hpp"
#include "hit_geometry.hpp"

namespace {

using namespace framework::brawl;
using test::check;

const fix32 RAW = fix32::from_raw(1);

fix32 px(int32_t v) { return fix32::from_int(v); }

bool crosses(const Body& a, const Body& t) { return rects_cross(place_box(a, test::REACH), place_box(t, test::HURT)); }

Body at(fix32 x, fix32 y, int8_t facing) {
    Body b;
    b.pos.x = x;
    b.pos.y = y;
    b.facing = facing;
    return b;
}

void edges_of(int8_t facing) {
    const Body a = at(px(100), px(40), facing);
    const HitRect r = place_box(a, test::REACH);
    const fix32 mid_x = r.x0 + px(6);
    const fix32 mid_y = r.h0 - px(10);
    check(!crosses(a, at(r.x1 + px(8), mid_y, 1)) && crosses(a, at(r.x1 + px(8) - RAW, mid_y, 1)),
          "the right edge of the hit box: touching misses, one raw inside hits");
    check(!crosses(a, at(r.x0 - px(8), mid_y, 1)) && crosses(a, at(r.x0 - px(8) + RAW, mid_y, 1)),
          "the left edge of the hit box: touching misses, one raw inside hits");
    check(!crosses(a, at(mid_x, r.h1, 1)) && crosses(a, at(mid_x, r.h1 - RAW, 1)),
          "the top edge of the hit box: touching misses, one raw inside hits");
    check(!crosses(a, at(mid_x, r.h0 - px(32), 1)) && crosses(a, at(mid_x, r.h0 - px(32) + RAW, 1)),
          "the bottom edge of the hit box: touching misses, one raw inside hits");
}

void test_edges() {
    const HitRect right = place_box(at(px(100), px(40), 1), test::REACH);
    const HitRect left = place_box(at(px(100), px(40), -1), test::REACH);
    check(right.x0 == px(108) && right.x1 == px(120) && right.h0 == px(58) && right.h1 == px(64),
          "facing right puts the box at x + rx, height y - ry");
    check(left.x0 == px(80) && left.x1 == px(92) && left.h0 == right.h0, "facing left mirrors the box about x");
    edges_of(1);
    edges_of(-1);
    check(!depths_cross(px(10), px(4), px(18), px(4)) && depths_cross(px(10), px(4), px(18) - RAW, px(4)),
          "depth: a gap equal to both thicknesses misses, one raw less hits");
    check(!depths_cross(px(18), px(4), px(10), px(4)) && depths_cross(px(18) - RAW, px(4), px(10), px(4)),
          "depth from the other side: the same edge");
}

struct Duel {
    test::Arena arena;
    BodyPool pool;
};

void face_off(Duel& d, bool right_first) {
    const Archetype& k = d.arena.kinds[0];
    if (right_first) d.pool.spawn(test::dummy(24, 0, -1, 1, k));
    d.pool.spawn(test::dummy(0, 0, 1, 0, k));
    if (!right_first) d.pool.spawn(test::dummy(24, 0, -1, 1, k));
}

void one_phase(BodyPool& pool, const test::Arena& arena, const std::vector<Command>& commands) {
    const BrawlWorld w = arena.world();
    for (uint32_t i = 0; i < pool.count; ++i) step_one(pool.bodies[i], commands[i], w);
    for (uint32_t i = 0; i < pool.count; ++i) {
        HitEvents events;
        collect_from(pool, i, w.kinds, w.rules, events);
        apply_hits(pool, w.kinds, events);
    }
}

int32_t mutual_hp(bool right_first, bool split) {
    Duel d;
    face_off(d, right_first);
    const std::vector<Command> jabs{test::strike(d.arena, d.arena.jab), test::strike(d.arena, d.arena.jab)};
    if (split) one_phase(d.pool, d.arena, jabs);
    else test::tick(d.pool, d.arena, jabs);
    for (int t = 0; t < 2; ++t) {
        if (split) one_phase(d.pool, d.arena, {Command{}, Command{}});
        else test::tick(d.pool, d.arena, {});
    }
    return d.pool.bodies[0].hp + d.pool.bodies[1].hp * 1000;
}

struct Trade {
    uint32_t kept = 0;
    uint32_t remembered = 0;
};

Trade mutual_trade() {
    Duel d;
    face_off(d, false);
    const std::vector<Command> jabs{test::strike(d.arena, d.arena.jab), test::strike(d.arena, d.arena.jab)};
    Trade out;
    for (int t = 0; t < 3; ++t) {
        HitEvents events;
        step_brawl(d.pool, t == 0 ? jabs : std::vector<Command>(2), d.arena.world(), events);
        for (uint32_t i = 0; i < events.count; ++i) {
            const HitEvent& e = events.at[i];
            out.kept += e.kept ? 1 : 0;
            const Body* a = d.pool.find(e.attacker);
            out.remembered += a != nullptr && a->struck.has(e.target, e.box) ? 1 : 0;
        }
    }
    return out;
}

void test_mutual_hit() {
    const Trade trade = mutual_trade();
    check(trade.kept == 2, "both hits of a mutual jab are marked kept on their events");
    check(trade.remembered == 0, "control: the struck list of a fighter hit in the same tick forgets his hit");
    check(mutual_hp(false, false) == 95095, "a mutual jab in one tick hits both fighters");
    check(mutual_hp(true, false) == 95095, "the mutual outcome does not depend on the seq order");
    check(mutual_hp(false, true) != 95095, "control: applying inside the collect loses the mutual hit");
}

int32_t jab_damage(bool forget_every_tick) {
    Duel d;
    d.pool.spawn(test::dummy(0, 0, 1, 0, d.arena.kinds[0]));
    d.pool.spawn(test::dummy(24, 0, -1, 1, d.arena.kinds[0]));
    test::tick(d.pool, d.arena, {test::strike(d.arena, d.arena.jab)});
    for (int t = 0; t < 8; ++t) {
        if (forget_every_tick) d.pool.bodies[0].struck = StruckList{};
        test::tick(d.pool, d.arena, {});
    }
    return 100 - d.pool.bodies[1].hp;
}

int32_t flurry_damage() {
    Duel d;
    d.pool.spawn(test::dummy(0, 0, 1, 0, d.arena.kinds[0]));
    d.pool.spawn(test::dummy(20, 0, -1, 1, d.arena.kinds[0]));
    test::tick(d.pool, d.arena, {test::strike(d.arena, d.arena.flurry)});
    for (int t = 0; t < 6; ++t) test::tick(d.pool, d.arena, {});
    return 100 - d.pool.bodies[1].hp;
}

void test_one_hit_per_activation() {
    check(jab_damage(false) == 5, "a box held over two frames hits its target once");
    check(jab_damage(true) > 5, "control: a struck list wiped every tick hits again");
    check(flurry_damage() == 14, "a box that leaves and comes back is a second activation");
}

void test_dropped_hit() {
    Duel d;
    Body& a = *d.pool.find(d.pool.spawn(test::dummy(0, 0, 1, 0, d.arena.kinds[0])));
    const Body& t = *d.pool.find(d.pool.spawn(test::dummy(24, 0, -1, 1, d.arena.kinds[0])));
    test::tick(d.pool, d.arena, {test::strike(d.arena, d.arena.jab)});
    for (uint32_t i = 0; i < MAX_STRUCK; ++i) a.struck.add(EntId{900 + i}, 1);
    HitEvents e;
    step_brawl(d.pool, std::vector<Command>(2), d.arena.world(), e);
    check(e.count == 1 && e.dropped == 1 && !e.at[0].kept && a.hitstop == 0 && t.hp == 100 && t.hitstop == 0,
          "a hit the struck list cannot remember neither lands, nor freezes anyone, nor is marked kept");
}

} // namespace

int main() {
    std::printf("brawl: hit boxes against hurt boxes\n");
    test::Arena probe;
    check(probe.error.empty(), "the dummy arena bakes and opens");
    if (!probe.error.empty()) std::printf("    %s\n", probe.error.c_str());
    test_edges();
    test_mutual_hit();
    test_one_hit_per_activation();
    test_dropped_hit();
    return test::verdict("framework-brawl-hit");
}
