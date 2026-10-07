#include "body_clip.hpp"

#include "clip.hpp"

namespace framework::brawl {

static_assert(MAX_HIT_BOXES == graphics::MAX_BOXES_OF_KIND, "every hit box index of a clip is re-armed");

namespace {

graphics::ClipView view_of(const Body& b, const Archetype& a) {
    return a.clips == nullptr ? graphics::ClipView{} : a.clips->clip(b.clip);
}

bool has_box(const graphics::ClipView& v, uint16_t frame, uint8_t index) {
    for (const graphics::ClipBox& box : graphics::frame_boxes(v, frame, graphics::BoxKind::Hit))
        if (box.index == index) return true;
    return false;
}

void switch_to(Body& b, uint16_t clip, uint32_t elapsed) {
    b.clip = clip;
    b.elapsed = elapsed;
    b.struck = StruckList{};
}

void enter(Body& b, const Archetype& a, uint16_t clip) {
    switch_to(b, clip, clip == a.jump ? airborne_ticks(b.pos, a.profile) : 0);
}

} // namespace

bool striking(const Body& b, const Archetype& a) { return is_move(a, b.clip); }

bool can_strike(const Body& b, const Archetype& a) { return b.hitstop == 0 && b.hitstun == 0 && !striking(b, a); }

void start_strike(Body& b, uint16_t clip) { switch_to(b, clip, 0); }

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

void advance_clip(Body& b, const Archetype& a) {
    ++b.elapsed;
    const graphics::ClipView v = view_of(b, a);
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
    const uint16_t want = locomotion_clip(b, a);
    if (!striking(b, a) && want != b.clip) enter(b, a, want);
}

} // namespace framework::brawl
