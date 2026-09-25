#include <cstdint>
#include <cstdio>
#include <vector>

#include "engine.hpp"
#include "mixer.hpp"

// Аудит #21 A·3·4: громкость на шве — доля в [0,1]. Выше единицы — усиление поверх полной шкалы, то
// есть срез и удар по ушам; ниже нуля — инверсия фазы вместо тишины. Сравнение с граничным
// значением, а не с голденом: утверждение «70000 звучит как 1» не зависит от формы сигнала.
using namespace audio;

namespace {

constexpr uint32_t FRAMES = 1024;
int fails = 0;

std::vector<int16_t> render(Backend b, fix32 voice_gain, fix32 bus_gain) {
    std::vector<int16_t> src(FRAMES);
    for (uint32_t i = 0; i < FRAMES; ++i) src[i] = static_cast<int16_t>((i % 61) * 300 - 9000);
    Mixer mix(b);
    mix.register_source(1, src.data(), FRAMES, nullptr);
    AudioEngine eng(mix);
    eng.set_bus_gain(Bus::Sfx, bus_gain, 0);
    PlayParams p;
    p.gain = voice_gain;
    eng.play(1, p, 0);
    std::vector<int16_t> out(FRAMES * OUT_CHANNELS);
    mix.mix(FRAMES, out.data());
    return out;
}

void expect_same(Backend b, fix32 vg, fix32 bg, fix32 want_vg, fix32 want_bg, const char* what) {
    if (render(b, vg, bg) != render(b, want_vg, want_bg)) {
        std::printf("[audio-gain] FAIL: %s (%s)\n", what, b == Backend::Fix32 ? "fix32" : "float");
        ++fails;
    }
}

} // namespace

int main() {
    const fix32 one = fix32::from_int(1), zero = fix32();
    const fix32 over = fix32::from_raw(70000), under = fix32::from_raw(-65536);
    for (Backend b : {Backend::Fix32, Backend::Float}) {
        expect_same(b, over, one, one, one, "voice gain above 1 plays as 1");
        expect_same(b, under, one, zero, one, "negative voice gain is silence");
        expect_same(b, one, over, one, one, "bus gain above 1 plays as 1");
        expect_same(b, one, under, one, zero, "negative bus gain is silence");
    }
    if (render(Backend::Fix32, one, one) == render(Backend::Fix32, zero, one)) {
        std::printf("[audio-gain] FAIL: the probe is silent, every comparison above is vacuous\n");
        ++fails;
    }
    if (fails) {
        std::printf("audio-gain: FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("audio-gain: PASS - 8 cases\n");
    return 0;
}
