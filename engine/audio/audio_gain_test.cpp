#include <cstdint>
#include <cstdio>
#include <vector>

#include "engine.hpp"
#include "mixer.hpp"

// Аудит #21 A·3·4 и A·3·3: громкость голоса, шины и мастера на шве — доля в [0,1]. Выше единицы — усиление поверх полной шкалы, то
// есть срез и удар по ушам; ниже нуля — инверсия фазы вместо тишины. Сравнение с граничным
// значением, а не с голденом: утверждение «70000 звучит как 1» не зависит от формы сигнала.
using namespace audio;

namespace {

constexpr uint32_t FRAMES = 1024;
int fails = 0;

std::vector<int16_t> render(Backend b, fix32 voice_gain, fix32 bus_gain,
                            fix32 master = fix32::from_int(1)) {
    std::vector<int16_t> src(FRAMES);
    for (uint32_t i = 0; i < FRAMES; ++i) src[i] = static_cast<int16_t>((i % 61) * 300 - 9000);
    Mixer mix(b);
    mix.register_source(1, src.data(), FRAMES, nullptr);
    AudioEngine eng(mix);
    eng.set_bus_gain(Bus::Sfx, bus_gain, 0);
    eng.set_master_gain(master, 0);
    PlayParams p;
    p.gain = voice_gain;
    eng.play(1, p, 0);
    std::vector<int16_t> out(FRAMES * OUT_CHANNELS);
    mix.mix(FRAMES, out.data());
    return out;
}

void check(bool same, Backend b, const char* what) {
    if (!same) {
        std::printf("[audio-gain] FAIL: %s (%s)\n", what, b == Backend::Fix32 ? "fix32" : "float");
        ++fails;
    }
}

} // namespace

int main() {
    const fix32 one = fix32::from_int(1), zero = fix32();
    const fix32 over = fix32::from_raw(70000), under = fix32::from_raw(-65536);
    for (Backend b : {Backend::Fix32, Backend::Float}) {
        check(render(b, over, one) == render(b, one, one), b, "voice gain above 1 plays as 1");
        check(render(b, under, one) == render(b, zero, one), b, "negative voice gain is silence");
        check(render(b, one, over) == render(b, one, one), b, "bus gain above 1 plays as 1");
        check(render(b, one, under) == render(b, one, zero), b, "negative bus gain is silence");
        check(render(b, one, one, over) == render(b, one, one, one), b, "master gain above 1 plays as 1");
        check(render(b, one, one, under) == render(b, one, one, zero), b,
              "negative master gain is silence");
    }
    if (render(Backend::Fix32, one, one) == render(Backend::Fix32, zero, one)) {
        std::printf("[audio-gain] FAIL: the probe is silent, every comparison above is vacuous\n");
        ++fails;
    }
    if (render(Backend::Fix32, one, one, fix32::from_raw(32768)) == render(Backend::Fix32, one, one)) {
        std::printf("[audio-gain] FAIL: master gain does not reach the mix, its cases are vacuous\n");
        ++fails;
    }
    if (fails) {
        std::printf("audio-gain: FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("audio-gain: PASS - 12 cases\n");
    return 0;
}
