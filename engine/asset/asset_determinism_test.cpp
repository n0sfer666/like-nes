#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/fixed.hpp"
#include "asset_manager.hpp"
#include "hash.hpp"

// Гейт #3 (спека #5): готовность ассета = ДЕТЕРМИНИРОВАННЫЙ gate. Замедленный async-I/O
// НЕ протекает в sim-hash (инвариант #2/#3 спеки #1). Sim (fix32) крутится параллельно
// со стримингом ассетов; тайминг диска варьируется (delay 0 vs большой) — sim-hash идентичен.
// Ключ: sim читает ready-set только в sync_point тика И НИКОГДА не пускает asset-байты/тайминг
// в своё состояние (assets кормят только рендер).

using namespace asset;

namespace {

constexpr int N_ENTITIES = 512;
constexpr int N_TICKS = 4000;

struct World {
    fix32 px[N_ENTITIES], py[N_ENTITIES], vx[N_ENTITIES], vy[N_ENTITIES];
};

void init(World& w) {
    for (int i = 0; i < N_ENTITIES; ++i) {
        w.px[i] = fix32::from_int(i % 64);
        w.py[i] = fix32::from_int(i / 64);
        w.vx[i] = fix32::from_raw(((i * 2654435761u) & 0xFFFF) - 0x8000);
        w.vy[i] = fix32::from_raw(((i * 40503u) & 0xFFFF) - 0x8000);
    }
}

void tick(World& w) {
    const fix32 dt = fix32::from_float(1.0 / 60.0);
    const fix32 g = fix32::from_float(9.8);
    const fix32 b = fix32::from_int(128), nb = -b;
    for (int i = 0; i < N_ENTITIES; ++i) {
        w.vy[i] = w.vy[i] + g * dt;
        w.px[i] = w.px[i] + w.vx[i] * dt;
        w.py[i] = w.py[i] + w.vy[i] * dt;
        if (b < w.px[i]) { w.px[i] = b; w.vx[i] = -w.vx[i]; }
        if (w.px[i] < nb) { w.px[i] = nb; w.vx[i] = -w.vx[i]; }
        if (b < w.py[i]) { w.py[i] = b; w.vy[i] = -w.vy[i]; }
        if (w.py[i] < nb) { w.py[i] = nb; w.vy[i] = -w.vy[i]; }
    }
}

uint64_t hash_world(const World& w) {
    uint64_t h = FNV_OFFSET;
    auto mix = [&](const fix32* a) {
        for (int i = 0; i < N_ENTITIES; ++i) {
            uint32_t u = static_cast<uint32_t>(a[i].raw);
            for (int bt = 0; bt < 4; ++bt) { h ^= (u >> (8 * bt)) & 0xFFu; h *= FNV_PRIME; }
        }
    };
    mix(w.px); mix(w.py); mix(w.vx); mix(w.vy);
    return h;
}

// Прогон sim, ПАРАЛЛЕЛЬНО стримя ассеты через менеджер с заданной задержкой I/O.
// Возвращает sim-hash; ready_seen — сколько ассетов реально догрузилось (стриминг был не no-op).
uint64_t run(const std::string& bundle, unsigned io_delay_us, int& ready_seen) {
    AssetManager am;
    if (!am.open(bundle, 8u * 1024 * 1024, /*trusted=*/false, io_delay_us)) {
        std::fprintf(stderr, "[asset-determinism] open failed\n");
        ready_seen = -1;
        return 0;
    }
    std::vector<uint64_t> guids = {
        fnv1a("hero_albedo", 11), fnv1a("hero_normal", 11),
        fnv1a("sprite.vs", 9), fnv1a("sprite.fs", 9), fnv1a("scene_bulk", 10)};
    for (uint64_t g : guids) am.request(g);

    World w;
    init(w);
    for (int t = 0; t < N_TICKS; ++t) {
        am.sync_point(); // публикация ready-set В sync-точке тика (детерм. gate)
        // ВАЖНО: is_ready() НЕ влияет на sim-состояние — только рендер бы его читал.
        tick(w);
    }
    const uint64_t sim_hash = hash_world(w); // хеш ФИКСИРУЕТСЯ до settle

    // Settle: даём стримингу догрузиться (вне sim-хеша) — доказываем, что I/O реально шёл
    // в обоих режимах, просто с разным таймингом. Тайминг сюда не влияет на sim выше.
    for (int i = 0; i < 2000; ++i) {
        am.sync_point();
        int r = 0;
        for (uint64_t g : guids) r += am.is_ready(g) ? 1 : 0;
        if (r == 5) break;
        usleep(500);
    }
    ready_seen = 0;
    for (uint64_t g : guids) ready_seen += am.is_ready(g) ? 1 : 0;
    return sim_hash;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: asset_determinism_test <bundle>\n"); return 2; }
    const std::string bundle = argv[1];

    int r_fast = 0, r_slow = 0;
    uint64_t h_fast = run(bundle, 0, r_fast);       // быстрый I/O
    uint64_t h_slow = run(bundle, 4000, r_slow);    // замедленный I/O (4ms/ассет)

    std::printf("[asset-determinism] sim-hash fast=0x%016llx slow=0x%016llx\n",
                (unsigned long long)h_fast, (unsigned long long)h_slow);
    std::printf("[asset-determinism] streamed fast=%d/5 slow=%d/5 (стриминг не no-op)\n",
                r_fast, r_slow);
    const bool ok = (h_fast == h_slow) && r_fast == 5 && r_slow == 5;
    std::printf("[asset-determinism] I/O-timing → sim-hash: %s\n", ok ? "INVARIANT (PASS)" : "FAIL");
    return ok ? 0 : 1;
}
