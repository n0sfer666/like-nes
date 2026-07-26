#include <cstdint>
#include <cstdio>
#include <vector>

#include "../asset/hash.hpp"
#include "engine.hpp"
#include "mixer.hpp"

// Гейт #1 (спека #3): байт-golden хеш микса. Fix32Mixer над ДЕТЕРМИНИРОВАННЫМ целочисленным
// PCM-входом + фикс. расписание команд → mix-буфер байт-идентичен run-to-run И cross-machine
// (только целочисл. математика: fix32-sat + integer sqrt, никаких float в детерм.-пути).

using namespace audio;

namespace {

// Детерминированный int16-тон (integer sawtooth) — без float/sin (cross-arch идентичность).
std::vector<int16_t> gen_tone(uint32_t frames, int period, int amp) {
    std::vector<int16_t> v(frames);
    for (uint32_t i = 0; i < frames; ++i) {
        int t = static_cast<int>(i % static_cast<uint32_t>(period));
        v[i] = static_cast<int16_t>((t * 2 * amp) / period - amp);
    }
    return v;
}

// Микс фикс. расписания в блоках block_frames. Sample-time команд одинаков → при
// sample-accurate миксе выход НЕ зависит от размера блока (инвариант #2).
uint64_t render_hash(uint32_t block_frames) {
    std::vector<int16_t> srcA = gen_tone(24000, 109, 18000);
    std::vector<int16_t> srcB = gen_tone(24000, 173, 12000);

    Mixer mix(Backend::Fix32);
    mix.register_source(1, srcA.data(), static_cast<uint32_t>(srcA.size()), nullptr);
    mix.register_source(2, srcB.data(), static_cast<uint32_t>(srcB.size()), nullptr);
    AudioEngine eng(mix);

    eng.set_listener(fix32(), fix32(), 0);
    eng.set_bus_gain(Bus::Music, fix32::from_float(0.8), 0);

    PlayParams sfx; // тон A справа (пан), Sfx-шина
    sfx.bus = Bus::Sfx; sfx.gain = fix32::from_int(1); sfx.x = fix32::from_int(4); sfx.priority = 200;
    eng.play(1, sfx, 0);

    PlayParams music; // тон B слева, Music-шина, loop
    music.bus = Bus::Music; music.gain = fix32::from_float(0.9); music.x = fix32::from_int(-4);
    music.loop = true; music.priority = 200;
    eng.play(2, music, 0);

    PlayParams duck; // на 10-м тике — ducking-голос (пригашает music)
    duck.bus = Bus::Sfx; duck.gain = fix32::from_int(1); duck.ducking = true; duck.priority = 250;
    eng.play(1, duck, static_cast<uint64_t>(SAMPLES_PER_TICK) * 10);

    PlayParams off; // старт на НЕ-кратном block sample-time → intra-block offset > 0 (sample-accuracy)
    off.bus = Bus::Ambience; off.gain = fix32::from_float(0.7); off.x = fix32::from_int(2); off.priority = 180;
    eng.play(1, off, 12345);

    const uint32_t total = SAMPLES_PER_TICK * 30; // 30 тиков
    std::vector<int16_t> out(static_cast<size_t>(total) * OUT_CHANNELS, 0);
    for (uint32_t pos = 0; pos < total; pos += block_frames) {
        uint32_t n = (total - pos < block_frames) ? total - pos : block_frames;
        mix.mix(n, out.data() + static_cast<size_t>(pos) * OUT_CHANNELS);
    }
    return asset::fnv1a(out.data(), out.size() * sizeof(int16_t));
}

} // namespace

int main() {
    uint64_t a = render_hash(SAMPLES_PER_TICK); // block=800
    uint64_t b = render_hash(SAMPLES_PER_TICK); // run-to-run
    uint64_t c = render_hash(256);              // другой размер блока → должен совпасть
    std::printf("[audio_golden] mix_hash = 0x%016llx\n", (unsigned long long)a);
    if (a != b) {
        std::fprintf(stderr, "[audio_golden] FAIL: run-to-run mismatch\n");
        return 1;
    }
    if (a != c) {
        std::fprintf(stderr, "[audio_golden] FAIL: block-size dependence 0x%016llx != 0x%016llx "
                     "(sample-accuracy broken)\n", (unsigned long long)a, (unsigned long long)c);
        return 1;
    }
    std::printf("[audio_golden] PASS byte-identical run-to-run AND block-size-independent "
                "(Fix32 sample-accurate mix)\n");
    return 0;
}
