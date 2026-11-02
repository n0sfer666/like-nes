#include "body_guard.hpp"

#include "body_clip.hpp"
#include "body_react.hpp"
#include "depth_step.hpp"

namespace framework::brawl {

namespace {

bool pressed(const BrawlInput& in, uint16_t bit) { return in.present && (in.buttons & bit) != 0; }

bool free_to_guard(const Body& b, const Archetype& a) {
    return (b.react == Reaction::None || b.react == Reaction::Block) && !striking(b, a) && grounded(b.pos);
}

} // namespace

bool track_guard(Body& b, const BrawlInput& in, const Archetype& a) {
    if ((b.react == Reaction::Block && !pressed(in, button::BLOCK)) ||
        (b.react == Reaction::Dodge && b.react_ticks == 0)) {
        b.react = Reaction::None;
        b.react_ticks = 0;
        halt_body(b);
    }
    if (!free_to_guard(b, a)) return false;
    if (pressed(in, button::DODGE)) {
        start_reaction(b, Reaction::Dodge, a.dodge_ticks, a);
        b.pos.vx = b.facing < 0 ? -a.run_x : a.run_x;
        b.pos.vz = fix32{};
        return true;
    }
    if (b.react != Reaction::None || !pressed(in, button::BLOCK)) return false;
    start_reaction(b, Reaction::Block, 0, a);
    halt_body(b);
    return true;
}

bool guards(const Body& target, const Body& attacker, const Strike& s) {
    if (target.react != Reaction::Block || (s.type != HitType::Light && s.type != HitType::Heavy)) return false;
    return target.facing < 0 ? !(target.pos.x < attacker.pos.x) : !(attacker.pos.x < target.pos.x);
}

void guard_strike(Body& b, fix32 knock_vx, const Strike& s) {
    if (!grounded(b.pos)) {
        b.react = Reaction::None;
        b.react_ticks = 0;
        return;
    }
    b.react_ticks = ticks16(s.hitstun);
    b.pos.vx = s.hitstun > 0 ? knock_vx / fix32::from_int(2) : fix32{};
    b.pos.vz = fix32{};
}

} // namespace framework::brawl
