#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"
#include "rumble_fighter.hpp"
#include "rumble_level.hpp"

namespace rumble {

void report_fighters(const Level& level, const Fighters& fighters, const Brawl& brawl);
void report_brawl(const Fighters& fighters, const Brawl& brawl, uint32_t tick);
void report_hits(const Brawl& brawl, uint32_t tick);

} // namespace rumble
