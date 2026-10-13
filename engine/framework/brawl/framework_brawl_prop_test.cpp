#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_conveyor_fixture.hpp"
#include "framework_brawl_grab_fixture.hpp"
#include "framework_brawl_path_fixture.hpp"
#include "framework_brawl_shot_fixture.hpp"
#include "framework_brawl_wave_fixture.hpp"
#include "stage_movers.hpp"

// Пропс (спека #25, В8г): удар даёт только урон — ни отброса, ни реакции, стоп-кадр бьющему; при
// `hp ≤ 0` — `Death`, как у бойца, и больше не цель; сам не ходит и не бьёт, захват его не берёт,
// волну не держит; в platform-секции входит при появлении; неуязвимость тикает; лента и лифт его
// не везут. Виды без клипа — пропса и снаряда — отказывают пустому боксу, захвату и глубине < 0.
namespace {

using namespace framework::brawl;
using namespace framework::brawl::test;
namespace graphics = framework::graphics;
namespace scene = framework::scene;

void takes_damage_only() {
    Shots w;
    const EntId a = w.fighter(100, 0);
    Body& by = *w.find(a);
    by.clip = w.sec.s.arena.jab;
    by.move = strike(w.sec.s.arena, w.sec.s.arena.jab).strike;
    by.elapsed = 1;
    const EntId c = w.crate(116, 30);
    HitEvents e;
    collect_hits(w.pool(), w.kinds, HitRules{}, e);
    apply_hits(w.pool(), w.kinds, e);
    const Body& crate = *w.find(c);
    check(e.count == 1 && e.at[0].target == c && crate.hp == 25, "a fighter's strike does not damage the prop");
    check(crate.pos.x == at(116) && crate.pos.vx == fix32{} && crate.pos.vy == fix32{} &&
              crate.react == Reaction::None && crate.hitstop == 0 && crate.clip == 0,
          "the prop is knocked, stopped or reacts to the strike");
    check(w.find(a)->hitstop == 3, "the strike on a prop does not stop its attacker");
}

void breaks_once() {
    Shots w;
    const EntId owner = w.fighter(100, 0);
    const EntId c = w.crate(140, 5);
    w.shoot(owner, aim(110, 4), 20);
    uint32_t t = 0;
    while (w.deaths.count == 0 && t < 20) {
        w.step();
        ++t;
    }
    check(w.deaths.count == 1 && w.deaths.at[0].body == c && w.hp(c) == 0, "a broken prop gives no death fact");
    const EntId next = w.shoot(owner, aim(110, 4), 20);
    for (int s = 0; s < 15; ++s) w.step();
    check(w.hp(c) == 0 && w.flying(next) && w.deaths.count == 0, "a broken prop is still a target");
}

void immunity_runs_out() {
    Shots w;
    const EntId c = w.crate(140, 5);
    w.find(c)->immune = 2;
    w.shoot(w.fighter(100, 0), aim(140, 0), 5);
    w.step();
    check(w.hp(c) == 5 && w.find(c)->immune == 1, "a prop takes a strike while immune or keeps its immunity");
    w.step();
    check(w.hp(c) == 0 && w.deaths.count == 1, "a prop stays immune after its immunity runs out");
}

void stands_still() {
    Shots w;
    const EntId c = w.crate(200, 30);
    w.find(c)->pos.vx = at(3);
    for (int t = 0; t < 10; ++t) w.step();
    check(w.find(c)->pos.x == at(200) && w.find(c)->age == 10,
          "a prop moves by itself");
}

bool rides_belt(int32_t x, int32_t z, const scene::Conveyor& belt) {
    Shots w;
    w.sec.s.level.conveyors.push_back(belt);
    const EntId c = w.crate(x, 30, z, 0);
    const EntId control = w.fighter(x + 22, 0, z);
    w.step();
    const fix32 from = w.find(control)->pos.x;
    const EntId shot = w.shoot(control, aim(x, 0, z), 30);
    for (int t = 0; t < 8; ++t) w.step();
    const bool still = w.find(c)->pos.x == at(x) && w.flying(shot) && w.find(shot)->pos.x == at(x);
    return still && from < w.find(control)->pos.x;
}

void belts_carry_no_props() {
    check(rides_belt(150, 250, lane(80, 200, 300, 300, 2)), "a belt carries a prop or a shot, or not its control");
    check(rides_belt(484, FLOOR, lane(470, 232, 540, 248, 2)),
          "a belt in platform carries a prop or a shot, or not its control");
}

void lifts_carry_no_props() {
    Shots w;
    scene::Path p = path({{500, 248}, {500, 196}}, 1, false);
    p.w = 96;
    w.sec.s.level.paths.push_back(p);
    StageMovers m(w.sec.s.level);
    w.sec.platform.scene.world = &m.world;
    w.movers = &m;
    const EntId c = w.crate(470, 30, FLOOR);
    const EntId control = w.fighter(530, 0, FLOOR);
    for (int t = 0; t < 60; ++t) w.step();
    check(w.find(c)->pos.x == at(470) && w.find(c)->pos.z == at(FLOOR) && w.find(control)->pos.z < at(FLOOR - 40),
          "a lift carries a prop or not its control");
}

void grab_passes_a_prop() {
    for (const Role role : {Role::Fighter, Role::Prop}) {
        Grapple g;
        Archetype kinds[2] = {g.kind(), g.kind()};
        kinds[1].role = role;
        g.reaching(0, 1, 0);
        g.r.put(16, -1, 1).kind = 1;
        HitEvents e;
        collect_hits(g.r.pool, kinds, HitRules{}, e);
        check(e.count == (role == Role::Fighter ? 1u : 0u), "a grab takes a prop or misses the control fighter");
    }
}

bool clears_with(Role role) {
    Waves w(700);
    w.s.pool.spawn(dummy(650, 240, 1, 0, w.s.arena.kinds[0]));
    w.step();
    w.s.arena.kinds[0].role = role;
    w.foe();
    w.step();
    return w.facts.cleared;
}

void wave_passes_props() {
    check(!clears_with(Role::Fighter), "control: a fighter foe does not hold the wave");
    check(clears_with(Role::Prop) && clears_with(Role::Projectile), "a prop or a shot holds the wave");
}

void enters_platform() {
    Shots w;
    const EntId owner = w.fighter(500, 0, FLOOR);
    const EntId c = w.crate(560, 30, FLOOR);
    w.step();
    check(w.sec.platform_mode(c) && w.find(c)->pos.x == at(560), "a prop spawned in platform stays in belt");
    w.shoot(owner, aim(510, 4, FLOOR), 30);
    for (int t = 0; t < 20; ++t) w.step();
    check(w.hp(c) == 20 && w.find(c)->pos.x == at(560) && w.find(c)->pos.z == at(FLOOR),
          "a platform shot does not damage the prop or moves it");
}

void kind_refusals() {
    Archetype k;
    Strike grab = shot_strike();
    grab.type = HitType::Grab;
    check(!projectile_kind({0, 0, 0, 4}, shot_strike(), k) && !projectile_kind({0, 0, 4, 0}, shot_strike(), k) &&
              !projectile_kind(SHOT_BOX, grab, k),
          "a projectile kind takes an empty box or a grab");
    check(!prop_kind({0, 0, 0, 4}, at(4), k) && !prop_kind({0, 0, 4, 0}, at(4), k) && !prop_kind(CRATE_BOX, at(-1), k),
          "a prop kind takes an empty box or a negative depth");
    const Shots w;
    const Archetype& shot = w.kinds[SHOT];
    check(shot.role == Role::Projectile && shot.move_count == 1 && shot.moves[0].strike.box == 0 &&
              shot.moves[0].strike.damage == 10 && shot.box.kind == static_cast<uint8_t>(graphics::BoxKind::Hit) &&
              shot.clips == nullptr,
          "a projectile kind does not keep its box and strike under box 0");
    const Archetype& crate = w.kinds[CRATE];
    check(crate.role == Role::Prop && crate.move_count == 0 && crate.depth == at(4) &&
              crate.box.kind == static_cast<uint8_t>(graphics::BoxKind::Hurt),
          "a prop kind does not keep its hurt box and depth");
}

} // namespace

int main() {
    takes_damage_only();
    breaks_once();
    immunity_runs_out();
    stands_still();
    belts_carry_no_props();
    lifts_carry_no_props();
    grab_passes_a_prop();
    wave_passes_props();
    enters_platform();
    kind_refusals();
    return verdict("framework-brawl-prop");
}
