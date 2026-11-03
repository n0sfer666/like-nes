#include "audio_stereo_fixture.hpp"

#include <cstdio>

#include "hash.hpp"

namespace {
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("audio-stereo: FAIL %s\n", why); } }
bool oracle(const std::vector<int16_t>& output, bool loop, uint32_t start = 0, int divisor = 1) {
    if (output.size() != 512) return false;
    for (uint32_t i = 0; i < 256; ++i) {
        // Независимая задержка47frames и явно заданный период5, без вызова Limiter/fetch.
        audio::StereoFrame expected;
        if (i >= 47 + start && (loop || i - 47 - start < 5)) expected = audio_stereo_fixture::PCM[(i - 47 - start) % 5];
        if (output[i * 2] != expected.left / divisor || output[i * 2 + 1] != expected.right / divisor) return false;
    }
    return true;
}
}
int main() {
    using namespace audio;
    using audio_stereo_fixture::render;
    const auto baseline = render(Backend::Fix32);
    const auto hash = asset::fnv1a(baseline.data(), baseline.size() * sizeof(int16_t));
    check(hash == 0x24a69db0fc56de9full, "independent Python LE stereo output oracle");
    std::printf("audio-stereo: mix hash=%016llx\n", static_cast<unsigned long long>(hash));
    for (Backend backend : {Backend::Fix32, Backend::Float}) {
        check(oracle(render(backend), true), "L/R identity without pan or distance attenuation");
        check(oracle(render(backend, 17, false), false), "resident end produces silence, no out-of-bounds frame");
        for (uint32_t block : {1u, 17u, 256u, 800u}) {
            check(render(backend, block) == baseline, "stereo mix independent of callback block size");
            check(oracle(render(backend, block, true, fix32::from_int(1), fix32::from_int(1), fix32::from_int(1), 17), true, 17),
                  "sample-accurate intra-block stereo start");
        }
        const auto half = fix32::from_raw(32768), one = fix32::from_int(1), zero = fix32();
        check(oracle(render(backend, 256, true, half, half, half), true, 0, 8), "voice/bus/master gains reach both channels");
        check(render(backend, 256, true, fix32::from_int(2)) == render(backend), "voice gain above1 clamps");
        check(render(backend, 256, true, fix32::from_int(-1)) == render(backend, 256, true, zero), "negative voice gain silences");
        check(render(backend, 256, true, one, fix32::from_int(2)) == render(backend), "bus gain above1 clamps");
        check(render(backend, 256, true, one, fix32::from_int(-1)) == render(backend, 256, true, one, zero), "negative bus gain silences");
        check(render(backend, 256, true, one, one, fix32::from_int(2)) == render(backend), "master gain above1 clamps");
        check(render(backend, 256, true, one, one, fix32::from_int(-1)) == render(backend, 256, true, one, one, zero), "negative master gain silences");
    }
    const int16_t source[]{800, -1600, 2400};
    Mixer mixed(Backend::Fix32), mono(Backend::Fix32);
    for (Mixer* mix : {&mixed, &mono}) mix->register_source(2, source, 3, nullptr);
    check(mixed.register_stereo_source(1, audio_stereo_fixture::PCM.data(), 5), "mixed source registration");
    AudioEngine with_stereo(mixed), without_stereo(mono);
    PlayParams params; params.loop = true;
    const auto stopped = with_stereo.play(1, params, 0); with_stereo.stop(stopped, 0);
    with_stereo.play(2, params, 0); without_stereo.play(2, params, 0);
    std::array<int16_t, 512> output{}, expected{};
    mixed.mix(256, output.data()); mono.mix(256, expected.data());
    check(output == expected, "reused stereo voice does not contaminate mono PCM or gains");
    std::puts(fails ? "audio-stereo: FAIL" : "audio-stereo: PASS independent channels, gain, loop, timing and mono compatibility");
    return fails ? 1 : 0;
}
