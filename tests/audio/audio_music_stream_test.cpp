#include "music_stream.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "mixer.hpp"
#include "music_stream_fixture.hpp"
#include "vorbis_fixture.hpp"

namespace {
bool ogg_stream() {
    using namespace audio;
    auto encoded = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    std::array<MusicTrack, 1> tracks{};
    tracks[0].id = 0;
    tracks[0].encoded = encoded;
    tracks[0].frames = 4800; tracks[0].loop_start = 37; tracks[0].loop_end = 4000;
    tracks[0].fade_samples = 0;
    Mixer mixer(Backend::Fix32);
    MusicStream stream;
    std::string error;
    bool ok = stream.init(mixer, tracks, 3, error) && !stream.want(17) && stream.want(0);
    tracks = {}; // Metadata скопированы; encoded bytes остаются заимствованными до join worker.
    stream.tick(100); stream.tick(101);
    std::array<int16_t, 128 * 2> output{};
    mixer.mix(128, output.data());
    ok &= stream.stats().live_decoders == 0;
    stream.tick(102);
    uint64_t energy = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    for (uint32_t block = 0; block < 160 && std::chrono::steady_clock::now() < deadline; ++block) {
        mixer.mix(128, output.data());
        for (int16_t sample : output) energy += static_cast<uint64_t>(sample < 0 ? -int32_t{sample} : sample);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const MusicStats stats = stream.stats();
    ok &= energy > 0 && !stats.closed && !stats.decode_failures && stats.peak_decoders == 1;
    ok &= mixer.peak_voices() == 0;
    return ok && !vorbis_fixture::fails;
}

bool game_queue_and_ids() {
    using namespace audio;
    std::array<StereoFrame, 17> pcm{};
    std::array<MusicTrack, 17> tracks{};
    for (uint32_t i = 0; i < tracks.size(); ++i) {
        pcm[i] = {static_cast<int16_t>(1000 + i * 10),
                  static_cast<int16_t>(-1000 - static_cast<int32_t>(i) * 10)};
        tracks[i].id = i == 16 ? UINT64_MAX : i;
        tracks[i].pcm = {&pcm[i], 1}; tracks[i].frames = tracks[i].loop_end = 1;
        tracks[i].loop_start = 0; tracks[i].fade_samples = 0;
    }
    Mixer mixer(Backend::Fix32);
    MusicStream stream;
    std::string error;
    auto duplicate = tracks;
    duplicate[1].id = duplicate[0].id;
    bool ok = !stream.init(mixer, duplicate, 1, error) && stream.init(mixer, tracks, 1, error);
    tracks = {}; // Метаданные скопированы до старта; PCM остаётся заимствованным до join worker.
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP + 1; ++i) {
        ok &= stream.want(i == 16 ? UINT64_MAX : i);
        stream.tick(i);
    }
    ok &= !stream.want(999); // Неизвестный ID не стирает последнее устойчивое желание.
    std::array<int16_t, 2> first{};
    mixer.mix(1, first.data());
    stream.tick(MUSIC_REQUEST_CAP + 2); // повторяет устойчивое желание после освобождения слота очереди.
    std::array<int16_t, 128> output{};
    for (uint32_t block = 0; block < 180; ++block) {
        mixer.mix(64, output.data());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ok &= output[126] == 1160 && output[127] == -1160;
    stream.reset_intent();
    ok &= stream.want(0); stream.tick(0);
    bool new_epoch = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!new_epoch && std::chrono::steady_clock::now() < deadline) {
        mixer.mix(64, output.data());
        new_epoch = output[126] == 1000 && output[127] == -1000;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ok &= new_epoch; // Без reset старый high-water отвергал бы новый tick0 навсегда.
    return ok;
}
}

int main() {
    const bool ok = ogg_stream() && game_queue_and_ids();
    std::puts(ok ? "audio-music-stream: PASS actual Ogg, threshold, pending queue, IDs and metadata" :
        "audio-music-stream: FAIL actual Ogg, threshold, pending queue, IDs and metadata");
    return ok ? 0 : 1;
}
