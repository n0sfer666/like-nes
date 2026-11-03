#include "audio_stereo_fixture.hpp"

#include <cstdio>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

namespace {
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("audio-stereo-limits: FAIL %s\n", why); } }
}
int main() {
    using namespace audio;
    const auto* pcm = audio_stereo_fixture::PCM.data();
    const int16_t mono[]{100};
    Mixer mix(Backend::Fix32);
    check(!mix.register_stereo_source(1, nullptr, 1), "null PCM with nonzero length refused");
    check(mix.register_stereo_source(1, pcm, 5) && !mix.register_stereo_source(1, pcm, 5), "duplicate guid refuses without replacing source");
    mix.register_source(2, mono, 1, nullptr);
    check(!mix.register_stereo_source(2, pcm, 5), "stereo cannot shadow an existing mono guid");
    for (uint64_t guid = 3; guid <= MAX_SOURCES; ++guid) check(mix.register_stereo_source(guid, pcm, 5), "fill source registry");
    check(!mix.register_stereo_source(MAX_SOURCES + 1, pcm, 5), "full registry refuses without out-of-bounds store");
    AudioEngine engine(mix);
    PlayParams params; params.loop = true;
    check(engine.play(1, params, 0) != 0, "play original source after refusal");
    std::array<int16_t, 512> output{};
    mix.mix(256, output.data());
    check(output[94] == 800 && output[95] == -1600, "failed registry writes preserve original audible source");
    Mixer empty(Backend::Float);
    check(empty.register_stereo_source(0, nullptr, 0), "empty stereo source is valid silence");
    AudioEngine no_pcm(empty); no_pcm.play(0, params, 0);
    empty.mix(256, output.data());
    check(std::all_of(output.begin(), output.end(), [](int16_t sample) { return sample == 0; }), "empty source never dereferences PCM");
    framework::probe::in_hot = true;
    const bool control = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(control && framework::probe::allocs >= 2, "allocator plain/aligned positive controls");
    for (Backend backend : {Backend::Fix32, Backend::Float}) {
        Mixer rt(backend); check(rt.register_stereo_source(1, pcm, 5), "RT fixture");
        AudioEngine producer(rt);
        // Переполнение32голосов и stereo→mono reuse также проходят внутри callback.
        rt.register_source(2, mono, 1, nullptr);
        for (uint32_t i = 0; i < MAX_VOICES + 8; ++i) {
            params.priority = static_cast<uint8_t>(i);
            check(producer.play(i % 2 ? 2 : 1, params, i * 3) != 0, "queued voice-stealing fixture");
        }
        framework::probe::allocs = 0; framework::probe::in_hot = true;
        for (uint32_t i = 0; i < 32; ++i) rt.mix(256, output.data());
        framework::probe::in_hot = false;
        check(framework::probe::allocs == 0 && rt.peak_voices() == MAX_VOICES, "both callbacks allocate nothing including command drain and stealing");
        check(rt.underruns() == 0, "resident stereo never counts a streaming underrun");
    }
    std::puts(fails ? "audio-stereo-limits: FAIL" : "audio-stereo-limits: PASS registry bounds, empty PCM, voice reuse and zero allocations");
    return fails ? 1 : 0;
}
