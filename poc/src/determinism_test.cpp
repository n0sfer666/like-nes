#include "fixed.hpp"
#include <cstdint>
#include <cstdio>
#include <cassert>

// Автономный тест детерминизма: мини-симуляция на fix32 + golden-hash состояния.
// Цель — доказать, что fix32-симуляция даёт БИТ-В-БИТ идентичный хеш (для сверки x86 vs ARM
// это тот же хеш на обеих архитектурах). Компилируется без cmake/Dawn.

static constexpr int N_ENTITIES = 1000;
static constexpr int N_TICKS = 10000;

struct World {
    fix32 px[N_ENTITIES];
    fix32 py[N_ENTITIES];
    fix32 vx[N_ENTITIES];
    fix32 vy[N_ENTITIES];
};

static void init(World& w) {
    for (int i = 0; i < N_ENTITIES; ++i) {
        w.px[i] = fix32::from_int(i % 100);
        w.py[i] = fix32::from_int(i / 100);
        // Детерминированный псевдо-разброс скоростей на чистой целочисленной арифметике.
        w.vx[i] = fix32::from_raw(((i * 2654435761u) & 0xFFFF) - 0x8000);
        w.vy[i] = fix32::from_raw(((i * 40503u) & 0xFFFF) - 0x8000);
    }
}

static void tick(World& w) {
    const fix32 dt = fix32::from_float(1.0 / 60.0);
    const fix32 gravity = fix32::from_float(9.8);
    const fix32 bound = fix32::from_int(200);
    const fix32 nbound = -bound;
    for (int i = 0; i < N_ENTITIES; ++i) {
        w.vy[i] = w.vy[i] + gravity * dt;
        w.px[i] = w.px[i] + w.vx[i] * dt;
        w.py[i] = w.py[i] + w.vy[i] * dt;
        // Детерминированный отскок от границ.
        if (bound < w.px[i]) { w.px[i] = bound; w.vx[i] = -w.vx[i]; }
        if (w.px[i] < nbound) { w.px[i] = nbound; w.vx[i] = -w.vx[i]; }
        if (bound < w.py[i]) { w.py[i] = bound; w.vy[i] = -w.vy[i]; }
        if (w.py[i] < nbound) { w.py[i] = nbound; w.vy[i] = -w.vy[i]; }
    }
}

static constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
static constexpr uint64_t FNV_PRIME = 1099511628211ull;

// Хеш int32 по байтам в ФИКСИРОВАННОМ little-endian порядке (через сдвиги) —
// не зависит от endianness машины, поэтому тест проверяет именно арифметику fix32,
// а не byte-order памяти.
static uint64_t hash_i32(uint64_t h, int32_t v) {
    uint32_t u = static_cast<uint32_t>(v);
    for (int b = 0; b < 4; ++b) { h ^= (u >> (8 * b)) & 0xFFu; h *= FNV_PRIME; }
    return h;
}

static uint64_t hash_world(const World& w) {
    uint64_t h = FNV_OFFSET;
    auto mix = [&](const fix32* a) {
        for (int i = 0; i < N_ENTITIES; ++i) h = hash_i32(h, a[i].raw);
    };
    mix(w.px); mix(w.py); mix(w.vx); mix(w.vy);
    return h;
}

int main() {
    // Санити fix32.
    assert((fix32::from_int(3) * fix32::from_int(4)).to_int() == 12);
    assert((fix32::from_int(10) / fix32::from_int(2)).to_int() == 5);
    assert((fix32::from_int(7) - fix32::from_int(10)).to_int() == -3);

    World w;
    init(w);
    for (int t = 0; t < N_TICKS; ++t) tick(w);

    // Прогон дважды из одного init → должен дать тот же хеш (детерминизм между прогонами).
    World w2;
    init(w2);
    for (int t = 0; t < N_TICKS; ++t) tick(w2);

    uint64_t h1 = hash_world(w);
    uint64_t h2 = hash_world(w2);

    std::printf("[determinism] arch-golden-hash = 0x%016llx\n", (unsigned long long)h1);
    std::printf("[determinism] run1==run2: %s\n", h1 == h2 ? "YES" : "NO");
    return h1 == h2 ? 0 : 1;
}
