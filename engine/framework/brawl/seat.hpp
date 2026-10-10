#pragma once
#include <array>
#include <cstdint>

#include "ent_id.hpp"

namespace framework::brawl {

constexpr uint32_t SEATS = 2;

// Место игрока в драке: присутствует ли он и каким телом. Устройство сюда не входит — оно локально
// и в сети #27 ничего не значит; присутствие приходит битом present во вводе тика.
struct Seat {
    bool present = false;
    EntId body;
};

struct Seats {
    std::array<Seat, SEATS> at{};
};

} // namespace framework::brawl
