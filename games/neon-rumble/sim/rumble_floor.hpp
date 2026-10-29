#pragma once

#include <cstdint>

#include "depth_floor.hpp"
#include "fixmath.hpp"

namespace rumble {

// Точка в полосе пола и не внутри стены: край стены — ещё пол.
bool on_floor(framework::depth::FloorView floor, fix32 x, fix32 z);

template <uint32_t N>
bool on_floor(const framework::depth::DepthFloor<N>& floor, fix32 x, fix32 z) {
    return on_floor(floor.view(), x, z);
}

} // namespace rumble
