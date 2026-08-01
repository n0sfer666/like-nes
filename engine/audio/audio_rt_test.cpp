#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

#include "engine.hpp"
#include "mixer.hpp"
#include "platform_noinline.hpp"

// Гейт #3 (спека #3): audio-callback RT-safe — НЕТ heap-аллокаций/локов/I/O в mix()-пути
// (декод и регистрация источников — вне callback). Глобальный operator new считает аллокации,
// пока выставлен флаг «внутри callback»; проверяем 0. Плюс SPSC lock-free. Гоняется под ASan/UBSan.

namespace {
bool g_in_cb = false;
long g_allocs = 0;
} // namespace

// Запрет инлайна обязателен, а не косметика — почему, см. platform_noinline.hpp.

PLATFORM_NOINLINE void* operator new(std::size_t n) {
    if (g_in_cb) ++g_allocs;
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
PLATFORM_NOINLINE void* operator new[](std::size_t n) { return operator new(n); }
PLATFORM_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete(void* p, std::size_t) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace audio;

int main() {
    if (!std::atomic<std::size_t>{}.is_lock_free()) {
        std::fprintf(stderr, "[audio_rt] FAIL: SPSC index atomics not lock-free\n");
        return 1;
    }

    std::vector<int16_t> src(9600);
    for (size_t i = 0; i < src.size(); ++i) src[i] = static_cast<int16_t>((i % 131) * 200 - 13000);

    Mixer mix(Backend::Float); // прод-путь
    mix.register_source(1, src.data(), static_cast<uint32_t>(src.size()), nullptr);
    AudioEngine eng(mix);

    // Расписание (вне cb): music-loop + ducking + 40 SFX (> MAX_VOICES=32 → voice-stealing
    // случится ВНУТРИ cb-региона при дренаже команд).
    PlayParams music; music.bus = Bus::Music; music.gain = fix32::from_float(0.8); music.loop = true;
    eng.play(1, music, 0);
    for (int k = 0; k < 40; ++k) {
        PlayParams p;
        p.bus = (k % 2) ? Bus::Sfx : Bus::Ambience;
        p.gain = fix32::from_float(0.6);
        p.x = fix32::from_int((k % 9) - 4);
        p.ducking = (k % 11 == 0);
        p.priority = static_cast<uint8_t>(50 + (k % 100));
        eng.play(1, p, static_cast<uint64_t>(k) * 300);
    }
    eng.set_bus_gain(Bus::Sfx, fix32::from_float(0.9), 0);

    std::vector<int16_t> buf(256 * OUT_CHANNELS);

    g_in_cb = true; // ← RT-регион: mix() не должен аллоцировать
    for (int b = 0; b < 160; ++b) mix.mix(256, buf.data()); // ~40960 фреймов, покрывает все команды
    g_in_cb = false;

    if (g_allocs != 0) {
        std::fprintf(stderr, "[audio_rt] FAIL: %ld heap allocations in callback path\n", g_allocs);
        return 1;
    }
    std::printf("[audio_rt] PASS no-alloc callback (voices peak=%u, underruns=%llu, lock-free SPSC)\n",
                mix.peak_voices(), (unsigned long long)mix.underruns());
    return 0;
}
