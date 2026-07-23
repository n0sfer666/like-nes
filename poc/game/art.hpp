#pragma once
#include <cstdint>
#include <vector>

namespace game {

struct Region { float u0, v0, u1, v1; };

struct Atlas {
    std::vector<uint8_t> px;
    uint32_t w = 0, h = 0;
    Region ship;
    Region star;
};

Atlas build_atlas();

} // namespace game
