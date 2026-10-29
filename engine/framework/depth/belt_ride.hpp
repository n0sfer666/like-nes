#pragma once
#include <cstdint>

#include "depth_floor.hpp"

namespace framework::depth {

// Belt: тело на полу сдвигается по `x` через стены и края полосы, упёршееся стоит.
void ride_belt(DepthBody& d, fix32 belt, FloorView f);

template <uint32_t N>
void ride_belt(DepthBody& d, fix32 belt, const DepthFloor<N>& f) {
    ride_belt(d, belt, f.view());
}

} // namespace framework::depth
