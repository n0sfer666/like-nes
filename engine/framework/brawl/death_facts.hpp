#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"

namespace framework::brawl {

struct Death {
    EntId body;
};

// Факты тика смертей для игры, вне хеша: их пересобирает каждый шаг. Тел в пуле не больше
// `POOL_CAPACITY`, поэтому переполнения нет.
struct DeathFacts {
    std::array<Death, POOL_CAPACITY> at{};
    uint32_t count = 0;
};

} // namespace framework::brawl
