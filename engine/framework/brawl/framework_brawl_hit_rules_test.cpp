#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Ring;
using test::walk_right;

bool bystander_moves(bool freeze_world) {
    Ring r;
    r.put(0, 1, 0);
    r.put(24, -1, 1);
    r.put(-300, 1, 0);
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    test::tick(r.pool, r.arena, {});
    const Body* b = r.pool.bodies.data();
    const bool stopped = b[0].hitstop == 3 && b[1].hitstop == 3 && b[2].hitstop == 0;
    if (freeze_world) r.pool.bodies[2].hitstop = 3;
    const fix32 x = b[2].pos.x;
    const uint32_t elapsed = b[0].elapsed;
    test::tick(r.pool, r.arena, {Command{}, Command{}, walk_right()});
    return stopped && b[0].elapsed == elapsed && b[1].pos.x == fix32::from_int(24) && !(b[2].pos.x == x);
}

uint16_t hitstop_after(uint16_t before) {
    Ring r;
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    t.hitstop = before;
    r.hit();
    return t.hitstop;
}

void test_hitstop() {
    check(bystander_moves(false), "hitstop freezes the attacker and the target, a bystander walks on");
    check(!bystander_moves(true), "control: a world-wide hitstop freezes the bystander");
    check(hitstop_after(5) == 5 && hitstop_after(1) == 3, "a new hitstop takes the larger value, not the sum");
}

void test_sum_and_top() {
    Ring r;
    Body& a = r.put(0, 1, 0);
    Body& b = r.put(48, -1, 0);
    Body& t = r.put(24, -1, 1);
    r.mid_jab(a);
    b.clip = r.arena.flurry;
    r.move(r.arena.jab).damage = 20;
    const HitEvents e = r.hit();
    check(e.count == 2 && t.hp == 73, "damage of every event on a target adds up");
    check(t.react == Reaction::Hurt && t.react_ticks == 10 && t.pos.vx == fix32::from_int(-2) && t.pos.vy == fix32::from_int(3),
          "knock and hitstun come from the event with the largest hitstun, not the largest damage");
}

fix32 tie_knock(bool left_first) {
    Ring r;
    if (left_first) r.put(0, 1, 0);
    r.put(48, -1, 0);
    if (!left_first) r.put(0, 1, 0);
    r.put(24, -1, 1);
    const Command jab = test::strike(r.arena.jab);
    test::tick(r.pool, r.arena, {jab, jab});
    test::tick(r.pool, r.arena, {});
    const Body& t = r.pool.bodies[2];
    return t.hp == 90 ? t.pos.vx : fix32{};
}

void test_tie() {
    check(tie_knock(true) == fix32::from_int(1), "a tie in hitstun takes the knock of the lower seq");
    check(tie_knock(false) == fix32::from_int(-1), "the lower seq wins the tie whichever side it stands on");
}

bool no_owner(const Body& a, const Body& t, const HitRules& rules) {
    return !(a.id == t.id) && (rules.friendly_fire || a.team != t.team);
}

uint32_t thrown_hits(uint8_t thrown_team, bool friendly_fire, HitFilter filter, bool& hit_owner) {
    Ring r;
    Body& p = r.put(26, -1, 0);
    Body& foe = r.put(24, -1, 1);
    Body& thrown = r.put(0, 1, thrown_team);
    thrown.owner = p.id;
    r.mid_jab(thrown);
    HitEvents e;
    collect_from(r.pool, 2, r.arena.kinds, HitRules{friendly_fire}, e, filter);
    hit_owner = false;
    uint32_t foes = 0;
    for (uint32_t i = 0; i < e.count; ++i) {
        hit_owner = hit_owner || e.at[i].target == p.id;
        foes += e.at[i].target == foe.id ? 1u : 0u;
    }
    return foes;
}

void test_teams() {
    Ring r;
    Body& a = r.put(0, 1, 0);
    r.put(24, -1, 0);
    r.mid_jab(a);
    check(r.collect().count == 0 && r.collect(true).count == 1, "a team mate is hit only with friendly fire on");
    bool owner = true;
    check(thrown_hits(0, false, may_hit, owner) == 1 && !owner, "a thrown enemy takes the thrower's team");
    check(thrown_hits(1, false, may_hit, owner) == 0, "control: a thrown enemy that keeps its team spares its own");
    check(thrown_hits(0, true, may_hit, owner) == 1 && !owner, "a thrown body never hits its owner");
    thrown_hits(0, true, no_owner, owner);
    check(owner, "control: a filter without the owner hits the thrower under friendly fire");
}

void test_overflow() {
    Ring r;
    for (uint32_t i = 0; i < POOL_CAPACITY; ++i) r.mid_jab(r.put(i % 2 == 0 ? 0 : 24, i % 2 == 0 ? 1 : -1, i % 2));
    HitEvents e = r.collect();
    check(e.count == MAX_HIT_EVENTS && e.dropped == 13 * 13 * 2 - MAX_HIT_EVENTS,
          "the event buffer keeps 64 and counts the rest as dropped");
    Ring s;
    Body& a = s.put(0, 1, 0);
    for (int i = 0; i < 20; ++i) s.put(24, -1, 1);
    s.mid_jab(a);
    const HitEvents f = s.hit();
    uint32_t hurt = 0;
    for (uint32_t i = 1; i < s.pool.count; ++i) hurt += s.pool.bodies[i].hp < 100 ? 1u : 0u;
    check(f.count == 20 && f.dropped == 4 && hurt == MAX_STRUCK && a.struck.count == MAX_STRUCK,
          "a full struck list drops the hits it cannot remember");
}

uint16_t clip_after(uint16_t strike, int ticks) {
    Ring r;
    r.put(0, 1, 0);
    test::tick(r.pool, r.arena, {test::strike(strike)});
    for (int t = 1; t < ticks; ++t) test::tick(r.pool, r.arena, {});
    return r.pool.bodies[0].clip;
}

void test_clip_end() {
    Ring r;
    const uint16_t jab = r.arena.jab, flurry = r.arena.flurry, idle = r.arena.kinds[0].idle;
    check(clip_after(jab, 4) == jab && clip_after(jab, 5) == idle,
          "a strike ends at its period, not on its last frame");
    check(clip_after(flurry, 5) == flurry && clip_after(flurry, 6) == idle, "a looping strike ends at its period too");
}

void test_hitstun() {
    for (const uint16_t stun : {uint16_t{5}, uint16_t{0}}) {
        Ring r;
        Body& b = r.put(0, 1, 0);
        b.react = stun == 0 ? Reaction::None : Reaction::Hurt;
        b.react_ticks = stun;
        b.clip = stun == 0 ? b.clip : r.arena.kinds[0].hurt;
        Command c = walk_right();
        c.strike = r.arena.jab;
        test::tick(r.pool, r.arena, {c});
        const bool held = b.pos.x == fix32::from_int(0) && b.clip == r.arena.kinds[0].hurt && b.react_ticks == 4;
        check(stun == 0 ? b.clip == r.arena.jab && !held : held,
              stun == 0 ? "control: without hitstun the strike starts" : "hitstun ignores the input and the strike");
    }
}

} // namespace

int main() {
    std::printf("brawl: hit rules\n");
    test_hitstop();
    test_sum_and_top();
    test_tie();
    test_teams();
    test_overflow();
    test_clip_end();
    test_hitstun();
    return test::verdict("framework-brawl-hit-rules");
}
