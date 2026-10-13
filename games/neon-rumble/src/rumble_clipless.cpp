#include "rumble_clipless.hpp"

#include "clipless_kind.hpp"

namespace rumble {

namespace {

namespace br = framework::brawl;

constexpr framework::graphics::Rect16 KNIFE_BOX{-4, -56, 8, 4};
constexpr framework::graphics::Rect16 CRATE_BOX{-16, -40, 32, 40};
constexpr int32_t CRATE_DEPTH = 6;

br::Strike knife_strike() {
    br::Strike s;
    s.type = br::HitType::Light;
    s.damage = 8;
    s.depth = fix32::from_int(4);
    s.hitstop = 3;
    s.hitstun = 12;
    s.knock_x = fix32::from_int(1);
    return s;
}

} // namespace

void open_clipless(std::array<br::Archetype, KINDS>& types) {
    br::projectile_kind(KNIFE_BOX, knife_strike(), types[KNIFE]);
    types[KNIFE].moves[0].name = "throw";
    br::prop_kind(CRATE_BOX, fix32::from_int(CRATE_DEPTH), types[CRATE]);
}

} // namespace rumble
