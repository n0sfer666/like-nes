#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "device.hpp"
#include "mixer.hpp"
#include "music_stream.hpp"
#include "platform_fs.hpp"
#include "vorbis_reader.hpp"

namespace {
using Clock = std::chrono::steady_clock;
std::atomic<uint64_t> silent_frames{0};

void silence(void*, uint32_t frames, int16_t* output) {
    std::fill_n(output, frames * audio::OUT_CHANNELS, int16_t{0});
    silent_frames.fetch_add(frames, std::memory_order_release);
}

template<class Predicate> bool wait_for(Predicate predicate) {
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    while (!predicate() && Clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return predicate();
}

bool render_contract() {
    audio::MiniaudioDevice device;
    if (device.start(nullptr, nullptr, true)) { device.stop(); return false; }
    if (!device.start(silence, nullptr, true)) return false;
    const bool ok = wait_for([] { return silent_frames.load(std::memory_order_acquire) > 0; }) &&
        device.sample_rate() == audio::SAMPLE_RATE && !device.start(nullptr, nullptr, true);
    device.stop(); device.stop();
    return ok;
}

struct State {
    audio::Mixer& mixer;
    std::atomic<uint64_t> frames{0}, loud_callbacks{0};
};

void render(void* user, uint32_t frames, int16_t* output) {
    auto& state = *static_cast<State*>(user);
    state.mixer.mix(frames, output);
    for (uint32_t i = 0; i < frames * audio::OUT_CHANNELS; ++i) {
        if (output[i]) { state.loud_callbacks.fetch_add(1, std::memory_order_relaxed); break; }
    }
    state.frames.fetch_add(frames, std::memory_order_release);
}

bool run(audio::Backend backend) {
    std::vector<uint8_t> encoded;
    if (!platform::read_bytes(AUDIO_DEVICE_FIXTURE, encoded) || encoded.empty()) return false;
    audio::VorbisInfo info; std::string error;
    if (!audio::inspect_vorbis(encoded, info, error) || info.rate != audio::SAMPLE_RATE || info.channels != 2)
        return false;
    auto mixer = std::make_unique<audio::Mixer>(backend);
    auto stream = std::make_unique<audio::MusicStream>();
    audio::MusicTrack track;
    track.id = UINT64_MAX; track.encoded = encoded; track.frames = info.frames;
    track.loop_start = 37; track.loop_end = 4000; track.fade_samples = 0;
    if (!stream->init(*mixer, {&track, 1}, 1, error) || !stream->want(track.id)) return false;
    stream->tick(0);
    State state{*mixer};
    audio::MiniaudioDevice device;
    if (!device.start(render, &state, true)) return false;
    bool ok = wait_for([&] { return state.loud_callbacks.load(std::memory_order_acquire) > 0; });
    ok &= device.sample_rate() == audio::SAMPLE_RATE && stream->stats().peak_decoders == 1 &&
          !stream->stats().decode_failures;
    stream.reset(); // Worker joins with the actual device callback still running.
    encoded.clear(); encoded.shrink_to_fit();
    const auto closed_at = state.frames.load(std::memory_order_acquire);
    // Observe at least512 rendered frames, beyond the47-frame limiter tail and an in-flight block.
    ok &= wait_for([&] { return state.frames.load(std::memory_order_acquire) >= closed_at + 512; });
    const auto loud_after_tail = state.loud_callbacks.load(std::memory_order_acquire);
    const auto quiet_at = state.frames.load(std::memory_order_acquire);
    ok &= wait_for([&] { return state.frames.load(std::memory_order_acquire) >= quiet_at + 1024; });
    ok &= state.loud_callbacks.load(std::memory_order_acquire) == loud_after_tail;
    device.stop(); // Joins callbacks before Mixer and callback user-data can die.
    const auto stopped_at = state.frames.load(std::memory_order_acquire);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    ok &= state.frames.load(std::memory_order_acquire) == stopped_at;
    mixer.reset();
    return ok;
}
} // namespace

int main() {
    const bool contract = render_contract();
    const bool fixed = contract && run(audio::Backend::Fix32);
    const bool floating = contract && run(audio::Backend::Float);
    const bool ok = contract && fixed && floating;
    std::puts(ok ? "audio-music-device: PASS real null callbacks, stream shutdown and source lifetime" :
        "audio-music-device: FAIL real null callbacks, stream shutdown and source lifetime");
    return ok ? 0 : 1;
}
