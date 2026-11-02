#include "body_run.hpp"

#include "body_clip.hpp"
#include "depth_step.hpp"

namespace framework::brawl {

namespace {

int8_t sign_of(const BrawlInput& in) {
    if (!in.present) return 0;
    if (in.move.move_x < fix32{}) return -1;
    return fix32{} < in.move.move_x ? int8_t{1} : int8_t{0};
}

bool free_to_run(const Body& b, const Archetype& a) { return !striking(b, a) && b.react == Reaction::None; }

} // namespace

void track_run(Body& b, const BrawlInput& in, const Archetype& a) {
    RunState& r = b.run;
    const int8_t dir = sign_of(in);
    const bool pressed = dir != 0 && dir != r.held;
    const bool twice = pressed && r.tap == dir && r.tap_ticks > 0;
    if (r.tap_ticks > 0) --r.tap_ticks;
    if (pressed) {
        r.tap = dir;
        r.tap_ticks = twice ? uint8_t{0} : a.run_tap;
    }
    r.held = dir;
    if (twice && grounded(b.pos)) r.dir = dir;
    if (r.dir != dir || !free_to_run(b, a)) r.dir = 0;
}

DepthProfile run_profile(const Body& b, const Archetype& a) {
    DepthProfile p = a.profile;
    if (b.run.dir != 0) p.speed_x = a.run_x;
    return p;
}

void slide_strike(Body& b, const Archetype& a) {
    const bool slides = striking(b, a) && a.moves[b.move].strike.slides;
    b.pos.vx = slides ? (b.facing < 0 ? -a.run_x : a.run_x) : fix32{};
    b.pos.vz = fix32{};
}

} // namespace framework::brawl
