#include "hit_collect.hpp"

#include "body_react.hpp"
#include "clip.hpp"
#include "depth_step.hpp"
#include "hit_geometry.hpp"

namespace framework::brawl {

namespace {

using graphics::BoxKind;
using graphics::ClipBox;

struct Sweep {
    const BodyPool& pool;
    std::span<const Archetype> kinds;
    const HitRules& rules;
    HitFilter filter;
    HitEvents& out;
};

uint16_t frame_now(const graphics::ClipView& v, const Body& b) { return graphics::clip_frame_at(v.clip, b.elapsed); }

bool boxes_crossed(const HitRect& hit, fix32 depth_a, fix32 z_a, const Body& t, BoxKind kind,
                   std::span<const Archetype> kinds) {
    const graphics::ClipView v = body_clip(t, kinds);
    if (!depths_cross(z_a, depth_a, t.pos.z, kinds[t.kind].depth)) return false;
    for (const ClipBox& box : graphics::frame_boxes(v, frame_now(v, t), kind))
        if (rects_cross(hit, place_box(t, box.rect))) return true;
    return false;
}

bool grabbable(const Body& t) {
    return grounded(t.pos) && (t.react == Reaction::None || t.react == Reaction::Hurt || t.react == Reaction::Block);
}

bool open_to(const Body& t, const Strike& s) {
    if (s.type == HitType::Grab) return grabbable(t);
    return vulnerable(t) || (t.react == Reaction::Down && s.hits_down);
}

void sweep(const Sweep& w, const Body& a, const Body& side, const ClipBox& box, uint8_t kind, uint8_t move) {
    const Strike& s = w.kinds[kind].moves[move].strike;
    const HitRect hit = place_box(a, box.rect);
    const BoxKind against = s.type == HitType::Grab ? BoxKind::Push : BoxKind::Hurt;
    const bool across = a.react == Reaction::Thrown && a.pos.vx != fix32{};
    const bool backward = across ? a.pos.vx < fix32{} : a.facing < 0;
    const fix32 knock = backward ? -s.knock_x : s.knock_x;
    for (uint32_t j = 0; j < w.pool.count; ++j) {
        const Body& t = w.pool.bodies[j];
        if (t.kind >= w.kinds.size() || !open_to(t, s) || !w.filter(side, t, w.rules) || a.struck.has(t.id, box.index))
            continue;
        if (boxes_crossed(hit, s.depth, a.pos.z, t, against, w.kinds))
            w.out.push(HitEvent{a.id, t.id, box.index, knock, kind, move});
    }
}

bool find_throw(const Archetype& a, uint8_t box, uint8_t& slot) {
    for (uint32_t i = 0; i < a.move_count; ++i) {
        if (a.moves[i].strike.type != HitType::Throw || a.moves[i].strike.box != box) continue;
        slot = static_cast<uint8_t>(i);
        return true;
    }
    return false;
}

void sweep_thrown(const Sweep& w, const Body& a, const graphics::ClipView& v) {
    if (a.grip.kind >= w.kinds.size()) return;
    Body side = a;
    side.team = a.grip.team;
    side.owner = a.grip.by;
    for (const ClipBox& box : graphics::frame_boxes(v, frame_now(v, a), BoxKind::Hit)) {
        uint8_t move = 0;
        if (find_throw(w.kinds[a.grip.kind], box.index, move)) sweep(w, a, side, box, a.grip.kind, move);
    }
}

bool reaches(const Body& a, const Archetype& k, uint8_t move) {
    const MoveSlot& m = k.moves[move];
    return m.strike.type != HitType::Grab || (a.clip == m.clip && grounded(a.pos));
}

} // namespace

bool may_hit(const Body& attacker, const Body& target, const HitRules& rules) {
    if (attacker.id == target.id || attacker.owner == target.id) return false;
    return rules.friendly_fire || attacker.team != target.team;
}

graphics::ClipView body_clip(const Body& b, std::span<const Archetype> kinds) {
    if (b.kind >= kinds.size() || kinds[b.kind].clips == nullptr) return {};
    return kinds[b.kind].clips->clip(b.clip);
}

void collect_from(const BodyPool& pool, uint32_t slot, std::span<const Archetype> kinds, const HitRules& rules,
                  HitEvents& out, HitFilter filter) {
    const Body& a = pool.bodies[slot];
    const graphics::ClipView v = body_clip(a, kinds);
    const Sweep w{pool, kinds, rules, filter, out};
    if (a.react == Reaction::Thrown) {
        sweep_thrown(w, a, v);
        return;
    }
    for (const ClipBox& box : graphics::frame_boxes(v, frame_now(v, a), BoxKind::Hit)) {
        uint8_t move = 0;
        if (find_move(kinds[a.kind], a.move, box.index, move) && reaches(a, kinds[a.kind], move))
            sweep(w, a, a, box, a.kind, move);
    }
}

void collect_hits(const BodyPool& pool, std::span<const Archetype> kinds, const HitRules& rules, HitEvents& out) {
    for (uint32_t i = 0; i < pool.count; ++i) collect_from(pool, i, kinds, rules, out);
}

} // namespace framework::brawl
