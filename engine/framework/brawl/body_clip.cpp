#include "body_clip.hpp"

#include "clip.hpp"

namespace framework::brawl {

static_assert(MAX_HIT_BOXES == graphics::MAX_BOXES_OF_KIND, "every hit box index of a clip is re-armed");

namespace {

graphics::ClipView view_of(const Body& b, const Archetype& a) {
    return a.clips == nullptr ? graphics::ClipView{} : a.clips->clip(b.clip);
}

uint32_t forward_ticks(const graphics::Clip& c) {
    uint32_t total = 0;
    for (uint16_t f = 0; f < c.frame_count; ++f) total += c.frames[f].duration;
    return total;
}

bool has_box(const graphics::ClipView& v, uint16_t frame, uint8_t index) {
    for (const graphics::ClipBox& box : graphics::frame_boxes(v, frame, graphics::BoxKind::Hit))
        if (box.index == index) return true;
    return false;
}

void switch_to(Body& b, uint16_t clip, uint32_t elapsed) {
    b.clip = clip;
    b.move = NO_STRIKE;
    b.elapsed = elapsed;
    b.struck = StruckList{};
    b.chain = NO_CHAIN;
}

void enter(Body& b, const Archetype& a, uint16_t clip) {
    switch_to(b, clip, clip == a.jump ? airborne_ticks(b.pos, a.profile) : 0);
}

} // namespace

bool striking(const Body& b, const Archetype& a) { return b.move < a.move_count; }

bool can_strike(const Body& b, const Archetype& a) { return b.hitstop == 0 && b.react == Reaction::None && !striking(b, a); }

void start_strike(Body& b, uint16_t move, const Archetype& a) {
    switch_to(b, a.moves[move].clip, 0);
    b.move = move;
}

void play_clip(Body& b, uint16_t clip) { switch_to(b, clip, 0); }

void end_strike(Body& b, const Archetype& a) {
    if (striking(b, a)) enter(b, a, locomotion_clip(b, a));
}

uint32_t airborne_ticks(const DepthBody& d, const DepthProfile& p) {
    if (!(fix32{} < p.gravity)) return 0;
    const int32_t t = ((p.jump_vy - d.vy) / p.gravity).to_int();
    return t > 0 ? static_cast<uint32_t>(t) : 0;
}

uint16_t locomotion_clip(const Body& b, const Archetype& a) {
    if (fix32{} < b.pos.y) return a.jump;
    if (b.pos.vx.raw != 0 || b.pos.vz.raw != 0) return a.walk;
    return a.idle;
}

uint16_t reaction_clip(const Body& b, const Archetype& a) {
    switch (b.react) {
    case Reaction::Hurt: return a.hurt;
    case Reaction::Fall: return a.fall;
    case Reaction::Down: return a.down;
    case Reaction::Getup: return a.getup;
    case Reaction::Block: return a.block;
    case Reaction::Dodge: return a.dodge;
    case Reaction::None: break;
    }
    return locomotion_clip(b, a);
}

void advance_clip(Body& b, const Archetype& a) {
    const graphics::ClipView v = view_of(b, a);
    if (b.react != Reaction::None && b.elapsed + 1u >= forward_ticks(v.clip)) return;
    ++b.elapsed;
    if (striking(b, a) && b.elapsed >= graphics::clip_period(v.clip)) {
        end_strike(b, a);
        return;
    }
    const uint16_t now = graphics::clip_frame_at(v.clip, b.elapsed);
    const uint16_t before = graphics::clip_frame_at(v.clip, b.elapsed - 1u);
    for (uint8_t k = 0; k < MAX_HIT_BOXES; ++k)
        if (has_box(v, now, k) && !has_box(v, before, k)) b.struck.forget(k);
}

void settle_clip(Body& b, const Archetype& a) {
    const uint16_t want = reaction_clip(b, a);
    if (!striking(b, a) && want != b.clip) enter(b, a, want);
}

} // namespace framework::brawl
