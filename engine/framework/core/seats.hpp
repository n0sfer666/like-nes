#pragma once
#include <array>
#include <cstdint>

#include "ent_id.hpp"

namespace framework::core {

// Место игрока: присутствует ли он и каким телом. Устройство сюда не входит — оно локально и в сети
// #27 ничего не значит; присутствие приходит битом present во вводе тика.
struct Seat {
    bool present = false;
    EntId body;
};

template <uint32_t N = 2>
struct Seats {
    std::array<Seat, N> at{};
};

} // namespace framework::core
