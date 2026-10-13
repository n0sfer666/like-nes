#include "hit_collect.hpp"

#include "body_kind.hpp"
#include "body_react.hpp"
#include "clip.hpp"
#include "depth_step.hpp"
#include "hit_geometry.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

namespace {

using graphics::BoxKind;
using graphics::ClipBox;

struct Sweep {
    const BodyPool& pool;
    std::span<const Archetype> kinds;
    const HitRules& rules;
    HitFilter filter;
    const StageSections* sections;
    HitEvents& out;
};

bool in_platform(const Sweep& w, EntId id) { return w.sections != nullptr && w.sections->find(id) != nullptr; }

HitRect lift(HitRect r, fix32 z) { return HitRect{r.x0, r.x1, r.h0 - z, r.h1 - z}; }

// В platform глубины нет, `z` — линия опоры: у двух тел на разных уступах она разная, и судить её
// толщиной значило бы не пускать удар по стоящему выше. Поэтому пара platform сравнивается в
// экранной высоте `y − z` без проверки глубины.
bool boxes_crossed(const HitRect& hit, fix32 depth_a, fix32 z_a, const Body& t, BoxKind kind,
                   std::span<const Archetype> kinds, bool flat) {
    if (!flat && !depths_cross(z_a, depth_a, t.pos.z, kinds[t.kind].depth)) return false;
    const HitRect from = flat ? lift(hit, z_a) : hit;
    for (const ClipBox& box : body_boxes(t, kinds, kind)) {
        const HitRect to = place_box(t, box.rect);
        if (rects_cross(from, flat ? lift(to, t.pos.z) : to)) return true;
    }
    return false;
}

bool grabbable(const Body& t) {
    return grounded(t.pos) && (t.react == Reaction::None || t.react == Reaction::Hurt || t.react == Reaction::Block);
}

bool open_to(const Body& t, const Strike& s) {
    if (s.type == HitType::Grab) return grabbable(t);
    return vulnerable(t) || (t.react == Reaction::Down && s.hits_down);
}

bool target_of(const Body& t, const Strike& s, std::span<const Archetype> kinds) {
    const Role role = role_of(t, kinds);
    if (role == Role::Prop) return t.hp > 0 && s.type != HitType::Grab;
    return role == Role::Fighter && open_to(t, s);
}

fix32 knock_of(const Body& a, const Strike& s) {
    const bool across = a.react == Reaction::Thrown && a.pos.vx != fix32{};
    const bool backward = across ? a.pos.vx < fix32{} : a.facing < 0;
    return backward ? -s.knock_x : s.knock_x;
}

bool lands(const Sweep& w, const Body& a, const Body& side, const ClipBox& box, const Strike& s, const Body& t) {
    if (t.kind >= w.kinds.size() || t.immune > 0 || !target_of(t, s, w.kinds) || !w.filter(side, t, w.rules) ||
        a.struck.has(t.id, box.index))
        return false;
    const bool flat = in_platform(w, a.id);
    const BoxKind against = s.type == HitType::Grab ? BoxKind::Push : BoxKind::Hurt;
    return in_platform(w, t.id) == flat &&
           boxes_crossed(place_box(a, box.rect), s.depth, a.pos.z, t, against, w.kinds, flat);
}

void sweep(const Sweep& w, const Body& a, const Body& side, const ClipBox& box, uint8_t kind, uint8_t move) {
    const Strike& s = w.kinds[kind].moves[move].strike;
    for (uint32_t j = 0; j < w.pool.count; ++j) {
        const Body& t = w.pool.bodies[j];
        if (lands(w, a, side, box, s, t)) w.out.push(HitEvent{a.id, t.id, box.index, knock_of(a, s), kind, move});
    }
}

void shoot(const Sweep& w, const Body& a) {
    const ClipBox* box = strike_box(a, w.kinds);
    if (box == nullptr) return;
    const Strike& s = w.kinds[a.kind].moves[0].strike;
    for (uint32_t j = 0; j < w.pool.count; ++j) {
        const Body& t = w.pool.bodies[j];
        if (!lands(w, a, a, *box, s, t)) continue;
        w.out.push(HitEvent{a.id, t.id, box->index, knock_of(a, s), a.kind, 0});
        return;
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

void sweep_thrown(const Sweep& w, const Body& a) {
    if (a.grip.kind >= w.kinds.size()) return;
    Body side = a;
    side.team = a.grip.team;
    side.owner = a.grip.by;
    for (const ClipBox& box : body_boxes(a, w.kinds, BoxKind::Hit)) {
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
                  HitEvents& out, HitFilter filter, const StageSections* sections) {
    const Body& a = pool.bodies[slot];
    const Sweep w{pool, kinds, rules, filter, sections, out};
    const Role role = role_of(a, kinds);
    if (role == Role::Prop) return;
    if (role == Role::Projectile) {
        if (a.life > 0) shoot(w, a);
        return;
    }
    if (a.react == Reaction::Thrown) {
        sweep_thrown(w, a);
        return;
    }
    for (const ClipBox& box : body_boxes(a, kinds, BoxKind::Hit)) {
        uint8_t move = 0;
        if (find_move(kinds[a.kind], a.move, box.index, move) && reaches(a, kinds[a.kind], move))
            sweep(w, a, a, box, a.kind, move);
    }
}

void collect_hits(const BodyPool& pool, std::span<const Archetype> kinds, const HitRules& rules, HitEvents& out,
                  const StageSections* sections) {
    for (uint32_t i = 0; i < pool.count; ++i) collect_from(pool, i, kinds, rules, out, may_hit, sections);
}

} // namespace framework::brawl
