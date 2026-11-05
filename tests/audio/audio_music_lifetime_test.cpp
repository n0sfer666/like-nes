#include "music_stream.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "mixer.hpp"

namespace {
using namespace audio;
MusicTrack track(std::span<const StereoFrame> pcm) {
    MusicTrack value;
    value.id = UINT64_MAX; value.pcm = pcm; value.frames = static_cast<uint32_t>(pcm.size());
    value.loop_start = 0; value.loop_end = value.frames; value.fade_samples = 0;
    return value;
}
bool stream_before_mixer() {
    Mixer mixer(Backend::Fix32);
    std::vector<StereoFrame> samples(1, {1200, -1300});
    MusicTrack source = track(samples);
    auto stream = std::make_unique<MusicStream>();
    std::string error;
    if (!stream->init(mixer, {&source, 1}, 1, error) || !stream->want(UINT64_MAX)) return false;
    stream->tick(0);
    std::atomic<bool> closing{false}, stop{false}, heard{false};
    std::atomic<uint32_t> closed_frames{0};
    std::atomic<bool> quiet_after_tail{true};
    std::thread callback([&] {
        std::array<int16_t, 128 * 2> output{};
        while (!stop.load(std::memory_order_acquire)) {
            const bool was_closing = closing.load(std::memory_order_acquire);
            mixer.mix(128, output.data());
            for (int16_t value : output) if (value) heard.store(true, std::memory_order_relaxed);
            if (was_closing) {
                for (uint32_t i = 0; i < output.size(); ++i) {
                    if (closed_frames.load(std::memory_order_relaxed) >= 128 && output[i] != 0)
                        quiet_after_tail.store(false, std::memory_order_relaxed);
                }
                closed_frames.fetch_add(128, std::memory_order_release);
            }
            std::this_thread::yield();
        }
    });
    const auto heard_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!heard.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() < heard_deadline)
        std::this_thread::yield();
    const bool played = heard.load(std::memory_order_relaxed);
    stream.reset(); // Mixer сохраняет Runtime pin, поэтому callback может продолжиться.
    closing.store(true, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (closed_frames.load(std::memory_order_acquire) < 384 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    stop.store(true, std::memory_order_release);
    callback.join();
    samples.clear(); samples.shrink_to_fit();
    std::array<int16_t, 128 * 2> output{};
    mixer.mix(128, output.data());
    bool silent = true;
    for (int16_t value : output) silent &= value == 0;
    const bool tail = played && closed_frames.load() >= 384 && quiet_after_tail.load() && silent;
    return tail;
}
bool mixer_before_stream() {
    auto mixer = std::make_unique<Mixer>(Backend::Fix32);
    std::array<StereoFrame, 1> samples{{{500, -700}}};
    MusicTrack source = track(samples);
    auto stream = std::make_unique<MusicStream>();
    std::string error;
    if (!stream->init(*mixer, {&source, 1}, 1, error) || !stream->want(UINT64_MAX)) return false;
    stream->tick(0);
    std::array<int16_t, 128> output{};
    mixer->mix(64, output.data());
    const bool open = !stream->stats().closed;
    mixer.reset();
    const bool closed = stream->stats().closed && !stream->want(UINT64_MAX);
    stream->reset_intent(); stream->tick(1);
    stream.reset();
    return open && closed;
}
bool paused_full_queue() {
    Mixer mixer(Backend::Fix32);
    constexpr std::array<StereoFrame, 1> samples{{{1000, -1000}}};
    std::array<MusicTrack, 2> tracks{track(samples), track(samples)};
    tracks[1].id = 0;
    auto stream = std::make_unique<MusicStream>(); std::string error;
    if (!stream->init(mixer, tracks, 1, error) || !stream->want(UINT64_MAX)) return false;
    stream->tick(0);
    std::array<int16_t, 128> output{};
    bool heard = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!heard && std::chrono::steady_clock::now() < deadline) {
        mixer.mix(64, output.data());
        heard = output[126] != 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Callback стоит:16 accepted wishes и последующие pending не смогут получить RT ack.
    for (uint32_t tick = 1; tick <= 20; ++tick) {
        stream->want(tick & 1 ? 0 : UINT64_MAX); stream->tick(tick);
    }
    const auto before = std::chrono::steady_clock::now();
    stream.reset();
    return heard && std::chrono::steady_clock::now() - before < std::chrono::seconds(3);
}
}

int main() {
    const bool ok = stream_before_mixer() && mixer_before_stream() && paused_full_queue();
    std::puts(ok ? "audio-music-lifetime: PASS stream/mixer order, callback pin and borrowed source shutdown" :
        "audio-music-lifetime: FAIL stream/mixer order, callback pin and borrowed source shutdown");
    return ok ? 0 : 1;
}
