#include "machine.hpp"

namespace framework::graphics {
namespace {

AnimPlayer next_tick(const AnimPlayer& p) {
    AnimPlayer n = p;
    ++n.elapsed;
    return n;
}

bool transition_allowed(const AnimStateDef& s, const AnimPlayer& p) {
    if ((s.flags & ANIM_STATE_HOLD_UNTIL_DONE) != 0 && !anim_hold_expired(p)) return false;
    if ((s.flags & ANIM_STATE_HOLD_UNTIL_FRAME_END) != 0 && !anim_at_frame_end(p)) return false;
    return true;
}

} // namespace

bool anim_condition_holds(const AnimCondition& c, AnimFlags flags) {
    return (flags & c.all) == c.all && (flags & c.none) == 0;
}

bool anim_at_frame_end(const AnimPlayer& p) {
    if (clip_steps(p.clip) == 0) return true;
    const AnimPlayer n = next_tick(p);
    return clip_step_ordinal(p.clip, anim_time(p)) != clip_step_ordinal(n.clip, anim_time(n));
}

bool anim_hold_expired(const AnimPlayer& p) {
    const uint32_t period = clip_period(p.clip);
    if (period == 0) return true;
    return anim_time(next_tick(p)) >= period;
}

AnimStateId machine_pick(const AnimMachine& m, AnimFlags flags) {
    if (m.states == nullptr || m.current >= m.state_count) return ANIM_STATE_NONE;
    const AnimStateDef& s = m.states[m.current];
    AnimStateId best = ANIM_STATE_NONE;
    uint16_t best_priority = 0;
    for (uint16_t i = 0; i < s.transition_count; ++i) {
        const AnimTransition& t = s.transitions[i];
        if (t.to >= m.state_count) continue;
        if (!anim_condition_holds(t.when, flags)) continue;
        if (best != ANIM_STATE_NONE && t.priority <= best_priority) continue;
        best = t.to;
        best_priority = t.priority;
    }
    return best;
}

uint32_t machine_start(AnimMachine& m, AnimStateId to, AnimEvent* out, uint32_t max) {
    if (m.states == nullptr || to >= m.state_count) {
        m.current = ANIM_STATE_NONE;
        return 0;
    }
    m.current = to;
    return anim_play(m.player, m.states[to].clip, out, max);
}

uint32_t machine_step(AnimMachine& m, AnimFlags flags, AnimEvent* out, uint32_t max) {
    if (m.states == nullptr || m.current >= m.state_count) return 0;
    // Переход В СЕБЯ игнорируется: условие, которое держится, перезапускало бы клип каждым тиком и
    // намертво оставляло бы его на первом кадре. Перезапуск делается явно — `machine_start`.
    const AnimStateId to = machine_pick(m, flags);
    if (to != ANIM_STATE_NONE && to != m.current &&
        transition_allowed(m.states[m.current], m.player)) {
        return machine_start(m, to, out, max);
    }
    return anim_step(m.player, out, max);
}

uint16_t machine_frame(const AnimMachine& m) { return anim_frame(m.player); }

} // namespace framework::graphics
