#pragma once
#include <cstdint>
#include <span>

#include "seats.hpp"

namespace framework::physics {

uint64_t seats_hash(std::span<const core::Seat> seats);

template <uint32_t N>
uint64_t seats_hash(const core::Seats<N>& seats) {
    return seats_hash(std::span<const core::Seat>(seats.at));
}

} // namespace framework::physics
