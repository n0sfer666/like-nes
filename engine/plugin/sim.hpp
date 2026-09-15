#pragma once
#include "../asset/hash.hpp"
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

// Свой цикл здесь был четвёртой копией FNV семьи A (находка 5 аудита #21): те же константы, тот же
// фиксированный little-endian порядок байт. Голден sim_hash от сведения не сдвинулся.
inline uint64_t sim_hash_i32(uint64_t h, int32_t v) {
    return asset::fnv1a_u32(h, static_cast<uint32_t>(v));
}

inline uint64_t sim_hash(const SimWorld& w) {
    uint64_t h = asset::FNV_OFFSET;
    auto mix = [&](const fix32* a) {
        for (int i = 0; i < SimWorld::N; ++i) h = sim_hash_i32(h, a[i].raw);
    };
    mix(w.px); mix(w.py); mix(w.vx); mix(w.vy);
    return sim_hash_i32(h, w.tick);
}
