#include "brawl_step.hpp"

#include "body_clip.hpp"
#include "body_react.hpp"
#include "depth_step.hpp"
#include "hit_apply.hpp"
#include "hit_collect.hpp"

namespace framework::brawl {

namespace {

void move(Body& b, const BrawlInput& in, const Archetype& a, const DepthFloor& f) {
    const bool strikes = striking(b, a);
    if (strikes && grounded(b.pos)) {
        b.pos.vx = fix32{};
        b.pos.vz = fix32{};
    }
    if (strikes || b.react != Reaction::None) coast_body(b, a.profile, f);
    else step_body(b, in, a.profile, f);
}

} // namespace

void step_one(Body& b, const Command& cmd, const BrawlWorld& w) {
    if (b.kind >= w.kinds.size() || w.floor == nullptr) return;
    const Archetype& a = w.kinds[b.kind];
    if (b.hitstop > 0) {
        --b.hitstop;
        ++b.age;
        return;
    }
    if (cmd.strike != NO_STRIKE && can_strike(b, a) && is_move(a, cmd.strike)) start_strike(b, cmd.strike);
    else advance_clip(b, a);
    move(b, cmd.input, a, *w.floor);
    tick_reaction(b, a);
    settle_clip(b, a);
}

void step_brawl(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, HitEvents& events) {
    for (uint32_t i = 0; i < pool.count; ++i)
        step_one(pool.bodies[i], i < commands.size() ? commands[i] : Command{}, w);
    events.clear();
    collect_hits(pool, w.kinds, w.rules, events);
    apply_hits(pool, w.kinds, events);
}

} // namespace framework::brawl
