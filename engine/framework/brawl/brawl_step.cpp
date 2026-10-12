#include "brawl_step.hpp"

#include "body_chain.hpp"
#include "body_clip.hpp"
#include "body_guard.hpp"
#include "body_react.hpp"
#include "body_run.hpp"
#include "depth_step.hpp"
#include "hit_apply.hpp"
#include "hit_collect.hpp"
#include "platform_body.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

namespace {

void move(Body& b, const BrawlInput& in, const Archetype& a, const BrawlWorld& w, SectionBody* platform) {
    track_run(b, in, a);
    const bool strikes = striking(b, a);
    if (strikes && grounded(b.pos)) slide_strike(b, a);
    const bool held = strikes || b.react != Reaction::None;
    if (platform != nullptr && w.platform != nullptr) step_platform(b, in, held, *platform, *w.platform);
    else if (held) coast_body(b, a.profile, *w.floor);
    else step_body(b, in, run_profile(b, a), *w.floor);
}

} // namespace

void step_one(Body& b, const Command& cmd, const BrawlWorld& w, SectionBody* platform) {
    b.crushed = false;
    if (b.kind >= w.kinds.size() || w.floor == nullptr) return;
    const Archetype& a = w.kinds[b.kind];
    queue_strike(b, cmd.strike, a);
    if (b.immune > 0) --b.immune;
    if (b.hitstop > 0) {
        --b.hitstop;
        ++b.age;
        return;
    }
    const bool guarded = track_guard(b, cmd.input, a);
    if (!take_strike(b, a) && !guarded) advance_clip(b, a);
    move(b, cmd.input, a, w, platform);
    tick_reaction(b, a);
    settle_clip(b, a);
}

void step_bodies(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, StageSections* sections) {
    for (uint32_t i = 0; i < pool.count; ++i) {
        Body& b = pool.bodies[i];
        SectionBody* platform = sections != nullptr ? sections->find(b.id) : nullptr;
        step_one(b, i < commands.size() ? commands[i] : Command{}, w, platform);
    }
}

void resolve_hits(BodyPool& pool, const BrawlWorld& w, const StageSections* sections, HitEvents& events) {
    events.clear();
    collect_hits(pool, w.kinds, w.rules, events, sections);
    apply_hits(pool, w.kinds, events);
}

void step_brawl(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, HitEvents& events) {
    step_bodies(pool, commands, w, nullptr);
    resolve_hits(pool, w, nullptr, events);
}

} // namespace framework::brawl
