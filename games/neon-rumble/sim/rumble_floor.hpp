#pragma once

#include "depth_floor.hpp"
#include "fixmath.hpp"

namespace rumble {

// Точка в полосе пола и не внутри стены: край стены — ещё пол.
bool on_floor(const framework::brawl::DepthFloor& floor, fix32 x, fix32 z);

} // namespace rumble
