#include "body_react.hpp"

#include "body_clip.hpp"
#include "depth_step.hpp"

namespace framework::brawl {

void halt_body(Body& b) {
    b.pos.vx = fix32{};
    b.pos.vz = fix32{};
}

void start_reaction(Body& b, Reaction r, uint16_t ticks, const Archetype& a) {
    b.react = r;
    b.react_ticks = ticks;
    play_clip(b, reaction_clip(b, a));
}

bool vulnerable(const Body& b) {
    return b.react == Reaction::None || b.react == Reaction::Hurt || b.react == Reaction::Block;
}

void react_to(Body& b, const Strike& s, const Archetype& a) {
    if (s.type == HitType::Launch) start_reaction(b, Reaction::Fall, 0, a);
    else if (s.hitstun > 0) start_reaction(b, Reaction::Hurt, ticks16(s.hitstun), a);
}

void tick_reaction(Body& b, const Archetype& a) {
    if (b.react == Reaction::Fall) {
        if (!grounded(b.pos)) return;
        halt_body(b);
        start_reaction(b, Reaction::Down, a.down_ticks, a);
        return;
    }
    if (b.react == Reaction::None) return;
    if (b.react == Reaction::Block) {
        if (b.react_ticks > 0 && --b.react_ticks == 0) halt_body(b);
        return;
    }
    if (b.react == Reaction::Dodge) {
        if (b.react_ticks > 0) --b.react_ticks;
        return;
    }
    if (b.react_ticks > 0) --b.react_ticks;
    if (b.react_ticks > 0) return;
    if (b.react == Reaction::Down) {
        start_reaction(b, Reaction::Getup, a.getup_ticks, a);
        return;
    }
    b.react = Reaction::None;
    if (grounded(b.pos)) halt_body(b);
}

} // namespace framework::brawl
