#include "hit_collect.hpp"

#include "body_react.hpp"
#include "clip.hpp"
#include "hit_geometry.hpp"

namespace framework::brawl {

namespace {

using graphics::BoxKind;
using graphics::ClipBox;

uint16_t frame_now(const graphics::ClipView& v, const Body& b) { return graphics::clip_frame_at(v.clip, b.elapsed); }

bool hurt_crossed(const Body& a, const HitRect& hit, fix32 depth_a, const Body& t, std::span<const Archetype> kinds) {
    const graphics::ClipView v = body_clip(t, kinds);
    if (!depths_cross(a.pos.z, depth_a, t.pos.z, kinds[t.kind].depth)) return false;
    for (const ClipBox& hurt : graphics::frame_boxes(v, frame_now(v, t), BoxKind::Hurt))
        if (rects_cross(hit, place_box(t, hurt.rect))) return true;
    return false;
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
    for (const ClipBox& box : graphics::frame_boxes(v, frame_now(v, a), BoxKind::Hit)) {
        uint8_t move = 0;
        if (!find_move(kinds[a.kind], a.clip, box.index, move)) continue;
        const Strike& s = kinds[a.kind].moves[move].strike;
        const HitRect hit = place_box(a, box.rect);
        for (uint32_t j = 0; j < pool.count; ++j) {
            const Body& t = pool.bodies[j];
            if (t.kind >= kinds.size() || !vulnerable(t) || !filter(a, t, rules) || a.struck.has(t.id, box.index)) continue;
            if (!hurt_crossed(a, hit, s.depth, t, kinds)) continue;
            const fix32 knock = a.facing < 0 ? -s.knock_x : s.knock_x;
            out.push(HitEvent{a.id, t.id, box.index, knock, a.kind, move});
        }
    }
}

void collect_hits(const BodyPool& pool, std::span<const Archetype> kinds, const HitRules& rules, HitEvents& out) {
    for (uint32_t i = 0; i < pool.count; ++i) collect_from(pool, i, kinds, rules, out);
}

} // namespace framework::brawl
