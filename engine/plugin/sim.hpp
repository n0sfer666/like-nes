#pragma once
#include "../core/fixed.hpp"
#include <cstdint>

struct SimWorld {
    static constexpr int N = 256;
    fix32 px[N];
    fix32 py[N];
    fix32 vx[N];
    fix32 vy[N];
    int32_t tick;
};

inline void sim_init(SimWorld& w) {
    for (int i = 0; i < SimWorld::N; ++i) {
        w.px[i] = fix32::from_int(i % 16);
        w.py[i] = fix32::from_int(i / 16);
        w.vx[i] = fix32::from_raw(((i * 2654435761u) & 0xFFFF) - 0x8000);
        w.vy[i] = fix32::from_raw(((i * 40503u) & 0xFFFF) - 0x8000);
    }
    w.tick = 0;
}

static constexpr uint64_t SIM_FNV_OFFSET = 1469598103934665603ull;
static constexpr uint64_t SIM_FNV_PRIME = 1099511628211ull;

inline uint64_t sim_hash_i32(uint64_t h, int32_t v) {
    uint32_t u = static_cast<uint32_t>(v);
    for (int b = 0; b < 4; ++b) { h ^= (u >> (8 * b)) & 0xFFu; h *= SIM_FNV_PRIME; }
    return h;
}

inline uint64_t sim_hash(const SimWorld& w) {
    uint64_t h = SIM_FNV_OFFSET;
    auto mix = [&](const fix32* a) {
        for (int i = 0; i < SimWorld::N; ++i) h = sim_hash_i32(h, a[i].raw);
    };
    mix(w.px); mix(w.py); mix(w.vx); mix(w.vy);
    return sim_hash_i32(h, w.tick);
}
