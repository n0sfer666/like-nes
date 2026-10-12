#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_grab_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Grapple;
using test::Ring;

uint32_t hits_on(uint16_t target_immune, uint16_t attacker_immune) {
    Ring r;
    Body& a = r.put(0, 1, 0);
    r.mid_jab(a);
    a.immune = attacker_immune;
    r.put(24, -1, 1).immune = target_immune;
    return r.collect().count;
}

void test_an_immune_target_is_skipped() {
    check(hits_on(0, 0) == 1, "control: the jab reaches a target without immunity");
    check(hits_on(1, 0) == 0, "a target with immune ticks left is not collected");
    check(hits_on(0, 9) == 1, "an immune attacker still strikes");
}

uint32_t grabs_on(uint16_t immune) {
    Grapple g;
    g.reaching(0, 1, 0);
    g.r.put(16, -1, 1).immune = immune;
    return g.r.collect().count;
}

bool thrown_body_hits(uint16_t immune) {
    Grapple g;
    g.reaching(0, 1, 0);
    const Body& b = g.r.put(16, -1, 1);
    g.hit();
    Body& d = g.r.put(40, -1, 1);
    d.immune = immune;
    for (uint32_t k = 0; k < 60 && b.react == Reaction::Thrown && d.hp == 100; ++k)
        test::tick(g.r.pool, g.r.arena, {});
    return d.hp < 100;
}

void test_grabs_and_thrown_bodies_skip_it_too() {
    check(grabs_on(0) == 1 && grabs_on(1) == 0, "a grab passes over an immune target, control: it takes a plain one");
    check(thrown_body_hits(0) && !thrown_body_hits(200),
          "a thrown body passes over an immune target, control: it hits a plain one");
}

void test_immunity_counts_down_in_hitstop() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    b.hitstop = 5;
    b.immune = 3;
    step_one(b, Command{}, r.arena.world());
    check(b.hitstop == 4 && b.immune == 2, "the immune timer runs down in the same step as hitstop, frozen or not");
    step_one(b, Command{}, r.arena.world());
    step_one(b, Command{}, r.arena.world());
    step_one(b, Command{}, r.arena.world());
    check(b.immune == 0, "the immune timer stops at zero");
}

uint32_t skipped_ticks(uint16_t immune) {
    Ring r;
    Body& a = r.put(0, 1, 0);
    r.mid_jab(a);
    Body& t = r.put(24, -1, 1);
    t.immune = immune;
    uint32_t skipped = 0;
    while (skipped < 16) {
        step_one(t, Command{}, r.arena.world());
        if (r.collect().count != 0) break;
        ++skipped;
    }
    return skipped;
}

void test_n_ticks_skip_n_minus_one_collects() {
    check(skipped_ticks(0) == 0 && skipped_ticks(1) == 0, "immune 0 and 1 protect no tick: the step spends the last one");
    check(skipped_ticks(4) == 3, "immune N set between ticks skips the target in the next N - 1 collects");
}

} // namespace

int main() {
    std::printf("brawl: respawn immunity ticks\n");
    test::Arena probe;
    check(probe.error.empty(), "the dummy arena bakes and opens");
    if (!probe.error.empty()) std::printf("    %s\n", probe.error.c_str());
    test_an_immune_target_is_skipped();
    test_grabs_and_thrown_bodies_skip_it_too();
    test_immunity_counts_down_in_hitstop();
    test_n_ticks_skip_n_minus_one_collects();
    return test::verdict("framework-brawl-immune");
}
