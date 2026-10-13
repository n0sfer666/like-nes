#pragma once
#include "archetype.hpp"

namespace framework::brawl {

bool projectile_kind(const graphics::Rect16& box, const Strike& strike, Archetype& out);
bool prop_kind(const graphics::Rect16& hurt, fix32 depth, Archetype& out);

} // namespace framework::brawl
