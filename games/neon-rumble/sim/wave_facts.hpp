#pragma once
#include <array>
#include <cstdint>

#include "body_pool.hpp"

namespace framework::brawl {

// Факты тика волн для игры, вне хеша: их пересобирает каждый шаг. `starts` — индексы в
// `LevelObjects::waves` точек, чьи тела игра ставит в этом тике; не влезшие считает `dropped`.
struct WaveFacts {
    std::array<uint32_t, POOL_CAPACITY> starts{};
    uint32_t count = 0;
    uint32_t dropped = 0;
    bool locked = false;
    bool cleared = false;
    bool released = false;
};

} // namespace framework::brawl
