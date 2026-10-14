#pragma once
#include <cstdint>

#include "rumble_brawl.hpp"

namespace rumble {

// Место, севшее в этот тик (`was` — места до шага; респаун после нокаута сюда не входит): тело входа
// в конце тика, а в волне ещё вид камеры и режим секции тела — по ним гейт судит, что вошедший стоит в
// виде, а не у края секции за ним.
void report_joins(const Brawl& brawl, const framework::brawl::Seats& was, uint32_t tick);

} // namespace rumble
