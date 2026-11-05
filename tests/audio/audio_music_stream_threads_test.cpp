#include "music_stream.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include "mixer.hpp"
#include "music_stream_fixture.hpp"
#include "vorbis_fixture.hpp"

int main() {
    using namespace audio;
    auto encoded = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    auto tracks = music_stream_fixture::catalogue();
    tracks[0].encoded = encoded; tracks[0].pcm = {}; tracks[0].frames = 4800;
    tracks[0].loop_start = 37; tracks[0].loop_end = 4000;
    Mixer mixer(Backend::Fix32);
    MusicStream stream; std::string error;
    if (!stream.init(mixer, tracks, 1, error)) return 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    std::atomic<bool> stop{false}, callback_done{false}, failed{false}, heard{false};
    std::thread callback([&] {
        std::array<int16_t, 128 * 2> output{};
        while (!stop.load(std::memory_order_acquire)) {
            mixer.mix(128, output.data());
            for (int16_t value : output) if (value) heard.store(true, std::memory_order_relaxed);
            if (std::chrono::steady_clock::now() >= deadline) { failed.store(true); break; }
            std::this_thread::yield();
        }
        callback_done.store(true, std::memory_order_release);
    });
    std::thread observer([&] {
        MusicStats previous{}; uint64_t previous_total = 0;
        while (!callback_done.load(std::memory_order_acquire)) {
            const MusicStats current = stream.stats();
            const uint64_t total = mixer.underruns();
            if (current.underruns < previous.underruns || current.decode_failures < previous.decode_failures ||
                current.peak_decoders < previous.peak_decoders || current.closed ||
                current.live_decoders > MUSIC_SLOTS || current.peak_decoders > MUSIC_SLOTS ||
                total < previous_total || mixer.peak_voices() != 0) failed.store(true);
            previous = current; previous_total = total;
            if (std::chrono::steady_clock::now() >= deadline) { failed.store(true); break; }
        }
    });
    stream.want(0); stream.tick(0);
    while (!heard.load(std::memory_order_relaxed) && !callback_done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    if (!heard.load(std::memory_order_relaxed)) failed.store(true);
    stream.want(UINT64_MAX); stream.tick(1);
    while (stream.stats().peak_decoders < 2 && !callback_done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    if (stream.stats().peak_decoders != 2) failed.store(true);
    stream.reset_intent();
    for (uint32_t tick = 0; tick < 256 && !callback_done.load(std::memory_order_acquire); ++tick) {
        stream.want((tick / 16) & 1 ? UINT64_MAX : 0);
        if ((tick % 7) == 0 && stream.want(1234567)) failed.store(true);
        stream.tick(tick);
        (void)stream.stats();
        std::this_thread::yield();
    }
    stop.store(true, std::memory_order_release);
    callback.join(); observer.join();
    const MusicStats stats = stream.stats();
    const bool ok = !failed.load() && heard.load() && !stats.closed && stats.peak_decoders == MUSIC_SLOTS &&
                    !stats.decode_failures && !vorbis_fixture::fails;
    std::puts(ok ? "audio-music-stream-threads: PASS concurrent Ogg worker, callback, control and stats" :
        "audio-music-stream-threads: FAIL concurrent Ogg worker, callback, control and stats");
    return ok ? 0 : 1;
}
