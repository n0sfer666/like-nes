#include <cstdio>
#include <string>
#include <vector>

#include "body_hash.hpp"
#include "clip.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_hit_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;
using test::Ring;
namespace graphics = framework::graphics;

constexpr uint32_t STEPS = 60;
constexpr uint32_t FALL_TICKS = 13;

struct Launched {
    Ring r;
    Body* target = nullptr;

    explicit Launched(HitType type, int32_t height = 0) {
        Strike& s = r.move(r.arena.jab);
        s.type = type;
        s.knock_y = type == HitType::Launch ? fix32::from_int(3) : fix32{};
        r.mid_jab(r.put(0, 1, 0));
        target = &r.put(24, -1, 1);
        target->pos.y = fix32::from_int(height);
        r.hit();
    }
    Reaction step() {
        test::tick(r.pool, r.arena, {});
        return target->react;
    }
    uint16_t shown() const { return graphics::clip_frame_at(r.arena.clips.clip(target->clip).clip, target->elapsed); }
    void settle() {
        for (uint32_t k = 0; k < STEPS && target->react != Reaction::None; ++k) step();
    }
};

uint32_t count(const std::vector<Reaction>& seen, Reaction r) {
    uint32_t n = 0;
    for (const Reaction s : seen) n += s == r ? 1u : 0u;
    return n;
}

void test_launch_falls_lies_and_gets_up() {
    Launched l(HitType::Launch);
    const Archetype& a = l.r.arena.kinds[0];
    check(l.target->react == Reaction::Fall && l.target->clip == a.fall, "a launch starts the fall on its clip");
    std::vector<Reaction> seen;
    bool landed_down = true, rose = false;
    uint16_t getup_frame = 0;
    for (uint32_t k = 0; k < STEPS; ++k) {
        const Reaction before = l.target->react;
        seen.push_back(l.step());
        if (before == Reaction::Fall && seen.back() == Reaction::Down)
            landed_down = l.target->pos.y == fix32{} && l.target->pos.vx == fix32{} && l.target->clip == a.down;
        if (before == Reaction::Down && seen.back() == Reaction::Getup) rose = l.target->clip == a.getup;
        if (seen.back() == Reaction::Getup) getup_frame = l.shown();
    }
    check(count(seen, Reaction::Fall) == FALL_TICKS, "the fall lasts the hitstop and the flight until the body lands");
    check(getup_frame == 2, "a ping-pong getup holds its last frame, not one on the way back");
    check(landed_down, "the body lies down on the ground, at rest, on the down clip");
    check(count(seen, Reaction::Down) == a.down_ticks && a.down_ticks == 6, "the body lies exactly down ticks");
    check(rose && count(seen, Reaction::Getup) == a.getup_ticks && a.getup_ticks == 4,
          "the body gets up on its clip for exactly getup ticks");
    check(seen.back() == Reaction::None && l.target->clip == a.idle, "after the getup the body stands idle");
}

void test_hurt_again_restarts_the_timer() {
    test::Ring r;
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    t.react = Reaction::Hurt;
    t.react_ticks = 2;
    t.clip = r.arena.kinds[0].hurt;
    t.elapsed = 1;
    check(r.hit().count == 1, "control: the hurt body is hit");
    check(t.react == Reaction::Hurt && t.react_ticks == 8 && t.elapsed == 0, "a second hurt restarts its timer and clip");
}

void test_hit_in_the_air_falls() {
    Launched low(HitType::Light, 2);
    const Archetype& a = low.r.arena.kinds[0];
    check(low.target->react == Reaction::Fall && low.target->clip == a.fall, "any hit in the air drops the body");
    bool lay = false;
    for (uint32_t k = 0; k < STEPS && low.target->react != Reaction::None; ++k) lay |= low.step() == Reaction::Down;
    check(lay && low.target->pos.y == fix32{} && low.target->clip == a.idle, "a juggled body lies down, then rises");
    Launched high(HitType::Light);
    check(high.target->react == Reaction::Hurt, "control: the same light hit on the ground hurts");
    high.target->pos.vy = fix32::from_int(6);
    high.settle();
    check(fix32{} < high.target->pos.y && high.target->pos.vx != fix32{} && high.target->clip == a.jump,
          "a hurt that ends in the air keeps the knock and goes on falling on the jump clip");
}

void test_light_hurts_for_its_hitstun() {
    Launched l(HitType::Light);
    const Archetype& a = l.r.arena.kinds[0];
    check(l.target->react == Reaction::Hurt && l.target->clip == a.hurt, "a light hit starts the hurt clip");
    uint32_t hurt = 0;
    while (hurt < STEPS && l.step() == Reaction::Hurt) ++hurt;
    check(hurt + 1 == 3 + 8, "the hurt lasts the hitstop and then the hitstun of the strike");
    check(l.target->clip == a.idle, "after the hurt the body stands idle");
}

uint32_t events_against(Reaction react) {
    Ring r;
    r.mid_jab(r.put(0, 1, 0));
    Body& t = r.put(24, -1, 1);
    t.react = react;
    t.react_ticks = 5;
    return r.collect().count;
}

void test_only_standing_bodies_are_hit() {
    check(events_against(Reaction::Fall) == 0 && events_against(Reaction::Down) == 0 &&
              events_against(Reaction::Getup) == 0,
          "a falling, lying or rising body is not hit");
    check(events_against(Reaction::None) == 1 && events_against(Reaction::Hurt) == 1,
          "control: a standing or hurt body is hit");
}

bool strike_starts(Reaction react) {
    Ring r;
    Body& b = r.put(0, 1, 0);
    b.react = react;
    b.react_ticks = 3;
    test::tick(r.pool, r.arena, {test::strike(r.arena, r.arena.jab)});
    return b.clip == r.arena.jab;
}

void test_no_strike_while_rising() {
    check(!strike_starts(Reaction::Getup) && !strike_starts(Reaction::Down), "a lying or rising body cannot strike");
    check(strike_starts(Reaction::None), "control: a standing body strikes");
}

void test_short_clip_holds_its_last_frame() {
    Launched l(HitType::Light);
    std::vector<uint16_t> shown;
    for (uint32_t k = 0; k < 3 + 6; ++k) {
        l.step();
        shown.push_back(l.shown());
    }
    check(shown[3] == 1, "control: the hurt clip advances once the hitstop is over");
    check(shown.back() == 1 && l.target->clip == l.r.arena.kinds[0].hurt,
          "a looping reaction clip shorter than the reaction holds its last frame");
}

bool replays(void (*forget)(Body&)) {
    Launched l(HitType::Launch);
    for (uint32_t k = 0; k < STEPS && l.target->react != Reaction::Down; ++k) l.step();
    if (l.target->react != Reaction::Down) return forget != nullptr;
    l.step();
    const BodyPool snap = l.r.pool;
    for (uint32_t k = 0; k < 12; ++k) l.step();
    const uint64_t straight = state_hash(l.r.pool);
    l.r.pool = snap;
    if (forget != nullptr) forget(*l.r.pool.find(l.target->id));
    for (uint32_t k = 0; k < 12; ++k) l.step();
    return straight == state_hash(l.r.pool);
}

void test_restore_mid_reaction() {
    check(replays(nullptr), "a snapshot taken while lying replays to the straight-run hash");
    check(!replays([](Body& b) { b.react_ticks = 0; }), "control: a snapshot without the reaction timer diverges");
    check(!replays([](Body& b) { b.react = Reaction::None; }), "control: a snapshot without the reaction diverges");
}

std::string arena_without(const std::string& tag) {
    std::vector<graphics::ClipSrc> src = test::dummy_clips();
    std::erase_if(src, [&](const graphics::ClipSrc& c) { return c.name == "dummy/" + tag; });
    return test::Arena(src).error;
}

void test_archetype_needs_every_reaction_clip() {
    check(arena_without("none").empty(), "control: the full sheet makes an archetype");
    for (const std::string tag : {"hurt", "fall", "down", "getup"})
        check(arena_without(tag).starts_with("no clip 'dummy/" + tag + "' in the clips"),
              "an archetype without a reaction clip is refused with its name");
}

} // namespace

int main() {
    std::printf("brawl: hurt, fall, down and getup\n");
    test_launch_falls_lies_and_gets_up();
    test_light_hurts_for_its_hitstun();
    test_hurt_again_restarts_the_timer();
    test_hit_in_the_air_falls();
    test_only_standing_bodies_are_hit();
    test_no_strike_while_rising();
    test_short_clip_holds_its_last_frame();
    test_restore_mid_reaction();
    test_archetype_needs_every_reaction_clip();
    return test::verdict("framework-brawl-react");
}
