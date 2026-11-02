#include "body_kind.hpp"

#include "clip.hpp"
#include "hit_collect.hpp"

namespace framework::brawl {

Role role_of(const Body& b, std::span<const Archetype> kinds) {
    return b.kind < kinds.size() ? kinds[b.kind].role : Role::Fighter;
}

std::span<const graphics::ClipBox> body_boxes(const Body& b, std::span<const Archetype> kinds, graphics::BoxKind kind) {
    if (b.kind >= kinds.size()) return {};
    const Archetype& a = kinds[b.kind];
    if (a.clips != nullptr) {
        const graphics::ClipView v = body_clip(b, kinds);
        return graphics::frame_boxes(v, graphics::clip_frame_at(v.clip, b.elapsed), kind);
    }
    if (a.role == Role::Fighter || a.box.kind != static_cast<uint8_t>(kind)) return {};
    return {&a.box, 1};
}

const graphics::ClipBox* strike_box(const Body& b, std::span<const Archetype> kinds) {
    if (b.kind >= kinds.size() || kinds[b.kind].move_count == 0) return nullptr;
    const uint8_t index = kinds[b.kind].moves[0].strike.box;
    for (const graphics::ClipBox& box : body_boxes(b, kinds, graphics::BoxKind::Hit))
        if (box.index == index) return &box;
    return nullptr;
}

} // namespace framework::brawl
