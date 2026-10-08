#pragma once
#include <cstdint>

namespace framework::brawl {

enum class Reaction : uint8_t { None = 0, Hurt = 1, Fall = 2, Down = 3, Getup = 4 };

} // namespace framework::brawl
