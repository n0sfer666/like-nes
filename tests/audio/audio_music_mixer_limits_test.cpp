#include "music_stream_fixture.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "audio_engine.hpp"

namespace {
using namespace audio;
constexpr StereoFrame ONE{1, 1}, SILENCE{0, 0};
int fails;
void check(bool ok, const char* name) {
    if (!ok) { ++fails; std::printf("audio-music-mixer-limits: FAIL %s\n", name); }
}
void binding_and_voices() {
    music_stream_fixture::ManualRig rig(Backend::Fix32);
    check(rig.bound, "first runtime binds"); if (!rig.bound) return;
    Mixer other(Backend::Fix32);
    auto fresh = std::make_shared<MusicRuntime>(4);
    auto invalid = std::make_shared<MusicRuntime>(0), over = std::make_shared<MusicRuntime>(4097);
    auto closed = std::make_shared<MusicRuntime>(4);
    closed->transport().closed.store(true);
    check(!MusicRuntime::bind(other, {}) && !MusicRuntime::bind(other, invalid) &&
          !MusicRuntime::bind(other, over) && !MusicRuntime::bind(other, closed), "invalid/null/closed bind refuses without reserving Mixer");
    check(!MusicRuntime::bind(*rig.mixer, fresh), "occupied Mixer preserves its original runtime");
    check(!MusicRuntime::bind(other, rig.runtime), "runtime cannot bind to a second Mixer");
    check(MusicRuntime::bind(other, fresh), "failed occupied bind leaves a fresh runtime reusable");
    for (uint32_t i = 0; i < MAX_SOURCES; ++i)
        check(rig.mixer->register_stereo_source(i, &ONE, 1), "music does not consume SourceRegistry capacity");
    check(!rig.mixer->register_stereo_source(MAX_SOURCES, &ONE, 1), "resident registry still enforces its bound");
    rig.runtime->post({0, 0});
    std::array<int16_t, 256> output{};
    rig.mixer->mix(1, output.data()); rig.pump();
    rig.mixer->mix(128, output.data());
    const uint64_t first_frame = rig.mixer->cursor();
    AudioEngine engine(*rig.mixer);
    PlayParams params; params.loop = true; params.priority = 255;
    for (uint32_t i = 0; i < 40; ++i) check(engine.play(0, params, first_frame) != 0, "SFX steals within its own pool");
    engine.set_listener(fix32::from_int(1000), fix32(), first_frame);
    rig.mixer->mix(128, output.data());
    bool exact = true;
    for (uint32_t i = 0; i < 128; ++i) {
        const uint64_t f = first_frame + i;
        const auto pcm = music_stream_fixture::sample(0, f - 48);
        const int32_t sfx = f >= first_frame + 47 ? MAX_VOICES : 0;
        exact &= output[i * 2] == pcm.left + sfx && output[i * 2 + 1] == pcm.right + sfx;
    }
    check(exact && rig.mixer->peak_voices() == MAX_VOICES, "all32 SFX and unstealable unpanned music remain audible after40 plays");
}
uint64_t render_music(bool ducked) {
    music_stream_fixture::ManualRig rig(Backend::Fix32);
    if (!rig.ready || !rig.bound) return 0;
    rig.mixer->register_stereo_source(99, &SILENCE, 1);
    if (ducked) {
        AudioEngine engine(*rig.mixer);
        PlayParams params; params.loop = true; params.ducking = true;
        engine.play(99, params, 0);
    }
    rig.runtime->post({0, 0});
    std::array<int16_t, 512 * 2> output{};
    rig.mixer->mix(64, output.data()); rig.pump();
    rig.mixer->mix(512, output.data());
    uint64_t energy = 0;
    for (int16_t sample : output) energy += static_cast<uint64_t>(std::abs(static_cast<int32_t>(sample)));
    return energy;
}
void gains_and_duck() {
    music_stream_fixture::ManualRig rig(Backend::Fix32);
    rig.runtime->post({0, 0});
    AudioEngine engine(*rig.mixer);
    engine.set_bus_gain(Bus::Music, fix32(), 0);
    std::array<int16_t, 256> output{};
    rig.mixer->mix(128, output.data()); rig.pump();
    rig.mixer->mix(128, output.data());
    bool silent = true;
    for (int16_t sample : output) silent &= sample == 0;
    check(silent, "BusMusic gain applies after dedicated music contribution");
    engine.set_bus_gain(Bus::Music, fix32::from_int(1), rig.mixer->cursor());
    engine.set_master_gain(fix32(), rig.mixer->cursor());
    rig.mixer->mix(128, output.data());
    for (int16_t sample : output) silent &= sample == 0;
    check(silent, "master gain applies to music");
    check(render_music(true) < render_music(false), "ducking SFX attenuates the Music bus contribution");
}
void underrun_accounting(Backend backend) {
    SampleRing empty; empty.init(1);
    music_stream_fixture::ManualRig rig(backend);
    rig.mixer->register_source(99, nullptr, 0, &empty);
    AudioEngine engine(*rig.mixer);
    PlayParams params; params.loop = true;
    rig.runtime->post({0, 0});
    std::array<int16_t, 2> first{};
    rig.mixer->mix(1, first.data()); rig.pump();
    engine.play(99, params, rig.mixer->cursor());
    constexpr uint32_t frames = StereoRing::CAPACITY + 10;
    std::vector<int16_t> output(frames * 2);
    rig.mixer->mix(frames, output.data());
    check(rig.runtime->stats().underruns == 10, "runtime counts exact missing music slot frames");
    check(rig.mixer->underruns() == frames + 10, "Mixer adds music and legacy voice underruns exactly once per block");
}
}
int main() {
    binding_and_voices(); gains_and_duck(); underrun_accounting(Backend::Fix32); underrun_accounting(Backend::Float);
    std::puts(fails ? "audio-music-mixer-limits: FAIL" :
        "audio-music-mixer-limits: PASS binding, VoicePool isolation, bus/master gain and ducking");
    return fails ? 1 : 0;
}
