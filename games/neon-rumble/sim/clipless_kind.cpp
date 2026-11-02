#include "clipless_kind.hpp"

namespace framework::brawl {

namespace {

bool solid(const graphics::Rect16& r) { return r.w > 0 && r.h > 0; }

graphics::ClipBox box_of(graphics::BoxKind kind, const graphics::Rect16& r) {
    return graphics::ClipBox{static_cast<uint8_t>(kind), 0, 0, r};
}

} // namespace

bool projectile_kind(const graphics::Rect16& box, const Strike& strike, Archetype& out) {
    if (!solid(box) || strike.type == HitType::Grab) return false;
    out = Archetype{};
    out.role = Role::Projectile;
    out.box = box_of(graphics::BoxKind::Hit, box);
    out.moves[0].strike = strike;
    out.moves[0].strike.box = 0;
    out.move_count = 1;
    return true;
}

bool prop_kind(const graphics::Rect16& hurt, fix32 depth, Archetype& out) {
    if (!solid(hurt) || depth < fix32{}) return false;
    out = Archetype{};
    out.role = Role::Prop;
    out.box = box_of(graphics::BoxKind::Hurt, hurt);
    out.depth = depth;
    return true;
}

} // namespace framework::brawl
