#include "world_rng.hpp"

namespace framework::ai {

uint32_t next_u32(WorldRng& rng) {
    rng.state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = rng.state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<uint32_t>((z ^ (z >> 31)) >> 32);
}

// Умножение вместо остатка: без деления, и смещение распределения при n много меньше 2^32 ничтожно.
uint32_t below(WorldRng& rng, uint32_t n) {
    return static_cast<uint32_t>((static_cast<uint64_t>(next_u32(rng)) * n) >> 32);
}

} // namespace framework::ai
