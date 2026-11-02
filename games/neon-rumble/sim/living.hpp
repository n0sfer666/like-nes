#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"

namespace framework::brawl {

// Тела с `hp > 0` на начало тика, в порядке пула. Вне хеша и снапшота: снимается заново каждый тик,
// в том числе на ресиме.
struct Living {
    std::array<EntId, POOL_CAPACITY> at{};
    uint32_t count = 0;
};

} // namespace framework::brawl
