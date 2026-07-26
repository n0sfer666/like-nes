#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#include "../asset/hash.hpp"
#include "engine.hpp"
#include "mixer.hpp"

// Гейт #2 (спека #3): аудио — output-only, НЕ кормит сим. Sim (fix32) детерминированно эмитит
// команды в sample-time; audio-callback крутится ПАРАЛЛЕЛЬНО с переменным таймингом (delay).
// Тайминг устройства НЕ протекает в sim-hash (инвариант #2/#3). Связь строго one-way SPSC
// (sim=продюсер, mixer=консюмер) → TSan-чисто. Sim НИКОГДА не читает состояние микшера.

using namespace audio;

namespace {

constexpr int N_ENTITIES = 512;
constexpr int N_TICKS = 3000;

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
    for (int i = 0; i < N_ENTITIES; ++i) {
        w.vy[i] = w.vy[i] + g * dt;
        w.px[i] = w.px[i] + w.vx[i] * dt;
        w.py[i] = w.py[i] + w.vy[i] * dt;
    }
}

uint64_t hash_world(const World& w) {
    uint64_t h = asset::FNV_OFFSET;
    h = asset::fnv1a(w.px, sizeof(w.px), h);
    h = asset::fnv1a(w.py, sizeof(w.py), h);
    h = asset::fnv1a(w.vx, sizeof(w.vx), h);
    h = asset::fnv1a(w.vy, sizeof(w.vy), h);
    return h;
}

// Один прогон: sim + параллельный audio-callback с задержкой delay_us. Возвращает sim-hash.
uint64_t run(unsigned delay_us) {
    std::vector<int16_t> src(4800);
    for (size_t i = 0; i < src.size(); ++i) src[i] = static_cast<int16_t>((i % 97) * 300 - 14000);

    Mixer mix(Backend::Fix32);
    mix.register_source(1, src.data(), static_cast<uint32_t>(src.size()), nullptr);
    AudioEngine eng(mix);

    std::atomic<bool> done{false};
    std::thread audio([&] {
        std::vector<int16_t> buf(256 * OUT_CHANNELS);
        while (!done.load(std::memory_order_acquire)) {
            mix.mix(256, buf.data()); // консюмер SPSC; тайминг варьируется
            if (delay_us) usleep(delay_us);
        }
    });

    World w;
    init(w);
    for (int t = 0; t < N_TICKS; ++t) {
        tick(w);
        uint64_t st = static_cast<uint64_t>(t) * SAMPLES_PER_TICK;
        eng.set_listener(w.px[0], w.py[0], st);
        if (t % 7 == 0) { // детерм. эмиссия по состоянию сим (продюсер SPSC)
            PlayParams p;
            p.bus = Bus::Sfx;
            p.gain = fix32::from_float(0.5);
            p.x = w.px[t % N_ENTITIES];
            p.y = w.py[t % N_ENTITIES];
            p.priority = 100;
            eng.play(1, p, st);
        }
    }

    done.store(true, std::memory_order_release);
    audio.join();
    return hash_world(w);
}

} // namespace

int main() {
    uint64_t fast = run(0);
    uint64_t slow = run(1500); // «медленный/джиттер» audio-callback

    std::printf("[audio_determinism] sim_hash fast=0x%016llx slow=0x%016llx\n",
                (unsigned long long)fast, (unsigned long long)slow);
    if (fast != slow) {
        std::fprintf(stderr, "[audio_determinism] FAIL: audio timing leaked into sim-hash\n");
        return 1;
    }
    std::printf("[audio_determinism] PASS invariant: audio callback timing does not perturb sim\n");
    return 0;
}
