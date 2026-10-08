#include <cstdio>
#include <string>
#include <vector>

#include "body_chain.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Ring;

struct Start {
    uint16_t clip;
    uint8_t chain;
};

std::vector<Start> starts(Ring& r, int ticks, int every, Body* away = nullptr) {
    std::vector<Start> out;
    const Archetype& a = r.arena.kinds[0];
    bool held = false;
    for (int t = 0; t < ticks; ++t) {
        test::tick(r.pool, r.arena, {t % every == 0 ? test::strike(r.arena.jab) : Command{}});
        const Body& b = r.pool.bodies[0];
        const bool fresh = is_move(a, b.clip) && b.elapsed == 0;
        if (fresh && !held) out.push_back({b.clip, b.chain});
        if (fresh && away && b.chain == 1) away->pos.x = fix32::from_int(200);
        held = fresh;
    }
    return out;
}

bool same(const std::vector<Start>& got, const std::vector<Start>& want) {
    bool ok = got.size() == want.size();
    for (std::size_t i = 0; ok && i < got.size(); ++i)
        ok = got[i].clip == want[i].clip && got[i].chain == want[i].chain;
    if (!ok)
        for (const Start& s : got) std::printf("    start clip %u chain %u\n", s.clip, s.chain);
    return ok;
}

void test_chain_on_hit() {
    Ring r;
    r.put(0, 1, 0);
    r.put(24, -1, 1);
    const uint16_t jab = r.arena.jab, flurry = r.arena.flurry;
    std::vector<Start> got = starts(r, 24, 2);
    got.resize(4, Start{NO_STRIKE, NO_CHAIN});
    check(same(got, {{jab, 0}, {jab, 1}, {flurry, 2}, {jab, 0}}),
          "a jab that lands chains into the next step, after the finisher the chain starts over");
}

void test_miss_restarts() {
    Ring r;
    r.put(0, 1, 0);
    r.put(200, -1, 1);
    const uint16_t jab = r.arena.jab;
    check(same(starts(r, 14, 2), {{jab, 0}, {jab, 0}, {jab, 0}}), "a jab that misses starts the chain over");
}

void test_miss_mid_chain() {
    Ring r;
    r.put(0, 1, 0);
    Body& target = r.put(24, -1, 1);
    const uint16_t jab = r.arena.jab;
    std::vector<Start> got = starts(r, 24, 2, &target);
    got.resize(3, Start{NO_STRIKE, NO_CHAIN});
    check(same(got, {{jab, 0}, {jab, 1}, {jab, 0}}), "a chain step that misses sends the next press back to the root");
}

void test_not_a_move() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    const uint16_t idle = r.arena.kinds[0].idle;
    test::tick(r.pool, r.arena, {test::strike(idle)});
    check(b.queued.strike == NO_STRIKE && b.clip == idle && b.elapsed > 0, "a clip that is not a move is not buffered");
}

uint32_t chained_at(std::size_t cancel_frame) {
    std::vector<framework::graphics::ClipSrc> src = test::dummy_clips();
    src[3] = test::cancels_at(test::dummy_clip("jab", {false, true, true, false}, framework::graphics::CLIP_ONCE),
                              cancel_frame);
    Ring r{test::Arena{src}, BodyPool{}};
    Body& b = r.put(0, 1, 0);
    r.put(24, -1, 1);
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    for (int t = 0; t < 12; ++t) {
        const uint32_t elapsed = b.elapsed;
        test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
        if (b.chain == 1) return elapsed;
    }
    return 99;
}

void test_cancel_window() {
    check(chained_at(1) == 1, "the chain fires as soon as the cancel frame is reached after a hit");
    check(chained_at(2) == 2, "the chain waits for the cancel frame even when the hit landed before it");
}

void test_buffer_expires() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    b.react = Reaction::Hurt;
    b.react_ticks = 12;
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    const uint16_t first = b.queued.ticks;
    for (int t = 0; t < 2; ++t) test::tick(r.pool, r.arena, {});
    check(first == 5 && b.queued.ticks == 3, "a queued strike loses a tick for every tick it cannot start");
    for (int t = 0; t < 14; ++t) test::tick(r.pool, r.arena, {});
    check(!is_move(r.arena.kinds[0], b.clip) && b.queued.ticks == 0 && b.queued.strike == NO_STRIKE,
          "a strike buffered longer than the buffer is dropped");
}

void test_buffer_starts_late() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    b.react = Reaction::Hurt;
    b.react_ticks = 3;
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    for (int t = 0; t < 4; ++t) test::tick(r.pool, r.arena, {});
    check(b.clip == r.arena.jab && b.chain == 0, "a strike buffered during a reaction starts once it ends");
}

void test_buffer_in_hitstop() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    b.hitstop = 10;
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    for (int t = 0; t < 9; ++t) test::tick(r.pool, r.arena, {});
    check(b.queued.ticks == r.arena.kinds[0].buffer_ticks, "hitstop freezes the buffer with the body");
    test::tick(r.pool, r.arena, {});
    check(b.clip == r.arena.jab && b.queued.ticks == 0, "the strike buffered in hitstop starts when it ends");
}

void test_other_strike_breaks() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    r.put(24, -1, 1);
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    for (int t = 0; t < 12 && b.clip != r.arena.flurry; ++t)
        test::tick(r.pool, r.arena, {test::strike(r.arena.flurry)});
    check(b.clip == r.arena.flurry && b.chain == NO_CHAIN,
          "a strike off the chain waits for the jab to end and leaves the chain");
}

void test_chain_cleared() {
    Ring r;
    Body& b = r.put(0, 1, 0);
    test::tick(r.pool, r.arena, {test::strike(r.arena.jab)});
    const uint8_t during = b.chain;
    for (int t = 0; t < 6; ++t) test::tick(r.pool, r.arena, {});
    check(during == 0 && b.clip == r.arena.kinds[0].idle && b.chain == NO_CHAIN,
          "a strike that ends leaves no chain step behind");
}

bool archetype_from(const std::vector<framework::graphics::ClipSrc>& clips, std::string& error) {
    std::vector<uint8_t> clip_bytes, fighter_bytes;
    FighterBakeError fe;
    framework::graphics::ClipTable table;
    FighterTable fighter;
    Archetype a;
    return framework::graphics::bake_clips(clips, clip_bytes, error) &&
           bake_fighter("dummy.fighter", test::DUMMY_TEXT, test::dummy_clips(), fighter_bytes, fe) &&
           table.open(clip_bytes.data(), clip_bytes.size()) &&
           fighter.open(fighter_bytes.data(), fighter_bytes.size()) && make_archetype(fighter, table, a, error);
}

void test_archetype_needs_cancel() {
    std::vector<framework::graphics::ClipSrc> plain = test::dummy_clips();
    plain[3].frames[1].event.clear();
    std::string error;
    check(archetype_from(test::dummy_clips(), error), "control: the clips the fighter was baked with make a kind");
    check(!archetype_from(plain, error) &&
              error == "chain step 0: clip 'dummy/jab' has no 'cancel' event to chain from",
          "clips that lost the cancel event of a chain step make no kind");
}

} // namespace

int main() {
    std::printf("brawl: a chain of strikes from a buffered input\n");
    test_chain_on_hit();
    test_miss_restarts();
    test_miss_mid_chain();
    test_not_a_move();
    test_cancel_window();
    test_buffer_expires();
    test_buffer_starts_late();
    test_buffer_in_hitstop();
    test_other_strike_breaks();
    test_chain_cleared();
    test_archetype_needs_cancel();
    return test::verdict("framework-brawl-chain");
}
