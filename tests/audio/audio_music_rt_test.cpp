#include "framework_alloc_probe.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include "framework_alloc_probe_control.hpp"
#include "music_stream_fixture.hpp"
#include "platform_mutex_probe.hpp"
#include "vorbis_fixture.hpp"

namespace {
using Clock = std::chrono::steady_clock;
namespace heap = framework::probe;
namespace mutex_probe = platform::mutex_probe;

bool measured_mix(audio::Mixer& mixer, uint32_t frames, int16_t* output) {
    heap::allocs = 0; heap::in_hot = true; mutex_probe::begin();
    mixer.mix(frames, output);
    const uint64_t locks = mutex_probe::end(); heap::in_hot = false;
    return !heap::allocs && !locks;
}

bool positive_controls() {
    heap::allocs = 0; heap::in_hot = true;
    const bool plain = heap::control::plain_allocation();
    const long plain_count = heap::allocs;
    heap::allocs = 0;
    const bool aligned = heap::control::aligned_allocation();
    const long aligned_count = heap::allocs;
    heap::in_hot = false;
    mutex_probe::begin();
    const bool exercised = mutex_probe::supported() && mutex_probe::exercise_mutexes();
    const uint64_t locks = mutex_probe::end();
    return plain && aligned && plain_count >= 2 && aligned_count >= 1 &&
        (mutex_probe::supported() ? exercised && locks == 2 : locks == 0);
}

// Cold controls run WHILE this thread is marked hot. No simultaneous allocation-control
// helpers: their shared escape pointer is intentionally not a concurrent test primitive.
bool cold_overlap() {
    std::atomic<bool> go{false}, done{false}; bool cold_ok = false;
    std::thread cold([&] {
        while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
        const bool allocated = heap::control::plain_allocation() && heap::control::aligned_allocation();
        const bool locked = !mutex_probe::supported() || mutex_probe::exercise_mutexes();
        cold_ok = allocated && locked && heap::allocs == 0 && mutex_probe::end() == 0;
        done.store(true, std::memory_order_release);
    });
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    heap::allocs = 0; heap::in_hot = true; mutex_probe::begin();
    go.store(true, std::memory_order_release);
    while (!done.load(std::memory_order_acquire) && Clock::now() < deadline) std::this_thread::yield();
    const bool overlapped = done.load(std::memory_order_acquire);
    const uint64_t locks = mutex_probe::end(); heap::in_hot = false;
    const long allocations = heap::allocs;
    cold.join();
    return overlapped && cold_ok && !locks && !allocations;
}

// The independent golden uses this timeline. Manual pump makes third/reverse/closed
// branches deterministic; the separate run below exercises a concurrent Ogg worker.
bool manual_timeline(audio::Backend backend) {
    music_stream_fixture::ManualRig rig(backend);
    if (!rig.bound) return false;
    bool ok = true; std::array<int16_t, 2> output{};
    for (uint32_t frame = 0; frame < 2300; ++frame) {
        if (frame == 0) ok &= rig.runtime->post({0, 100});
        if (frame == 200) ok &= rig.runtime->post({1, 1000});
        if (frame == 400) ok &= rig.runtime->post({2, 500});
        if (frame == 450) ok &= rig.runtime->post({3, 600});
        if (frame == 1000) ok &= rig.runtime->post({1, 1000});
        if (frame == 2200) rig.runtime->transport().closed.store(true, std::memory_order_release);
        ok &= measured_mix(*rig.mixer, 1, output.data());
        rig.pump();
    }
    return ok;
}

bool run(audio::Backend backend) {
    auto encoded = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    auto tracks = music_stream_fixture::catalogue();
    tracks[0].encoded = encoded; tracks[0].pcm = {}; tracks[0].frames = 4800;
    tracks[0].loop_start = 37; tracks[0].loop_end = 4000;
    audio::Mixer mixer(backend);
    auto stream = std::make_unique<audio::MusicStream>(); std::string error;
    if (!stream->init(mixer, tracks, 1, error)) return false;
    std::atomic<bool> stop{false}, done{false}, bad{false}, heard{false};
    std::atomic<uint32_t> blocks{0};
    const auto deadline = Clock::now() + std::chrono::seconds(12);
    std::thread callback([&] {
        if (!positive_controls()) bad.store(true, std::memory_order_relaxed);
        std::array<int16_t, 128 * 2> output{};
        while (!stop.load(std::memory_order_acquire)) {
            if (!measured_mix(mixer, 128, output.data())) bad.store(true, std::memory_order_relaxed);
            for (int16_t value : output) if (value) heard.store(true, std::memory_order_relaxed);
            blocks.fetch_add(1, std::memory_order_release);
            if (Clock::now() >= deadline) { bad.store(true); break; }
            std::this_thread::yield();
        }
        done.store(true, std::memory_order_release);
    });
    const auto wait_for = [&](auto predicate) {
        while (!predicate() && !done.load(std::memory_order_acquire) && Clock::now() < deadline)
            std::this_thread::yield();
        return predicate();
    };
    stream->want(0); stream->tick(0);
    bool ok = wait_for([&] { return heard.load(std::memory_order_relaxed); });
    stream->want(UINT64_MAX); stream->tick(1);
    ok &= wait_for([&] { return stream->stats().peak_decoders == 2; });
    const std::array<uint64_t, 6> wishes{0, UINT64_MAX, 12, 13, UINT64_MAX, 0};
    for (uint32_t tick = 2; tick < 98 && !done.load(std::memory_order_acquire); ++tick) {
        stream->want(wishes[(tick / 4) % wishes.size()]); stream->tick(tick);
        const uint32_t before = blocks.load(std::memory_order_acquire);
        ok &= wait_for([&] { return blocks.load(std::memory_order_acquire) >= before + 4; });
    }
    ok &= stream->stats().decode_failures == 0;
    stream.reset(); // Callback remains measured during close/join and subsequent closed reads.
    encoded.clear(); encoded.shrink_to_fit();
    const uint32_t closed_at = blocks.load(std::memory_order_acquire);
    ok &= wait_for([&] { return blocks.load(std::memory_order_acquire) >= closed_at + 4; });
    stop.store(true, std::memory_order_release); callback.join();
    return ok && !bad.load() && blocks.load() >= 388;
}
} // namespace

int main() {
    if (AUDIO_EXPECT_PTHREAD_PROBE && !mutex_probe::supported()) return 1;
    const bool controls = positive_controls() && cold_overlap();
    const bool fixed = controls && manual_timeline(audio::Backend::Fix32) && run(audio::Backend::Fix32);
    const bool floating = controls && manual_timeline(audio::Backend::Float) && run(audio::Backend::Float);
    std::puts(mutex_probe::supported() ? "audio-music-rt: pthread=intercepted" :
        "audio-music-rt: pthread=unavailable; allocation gate only on this platform");
    const bool ok = controls && fixed && floating && !vorbis_fixture::fails;
    std::puts(ok ? "audio-music-rt: PASS callback heap and mutex controls, cold overlap and concurrent stream" :
        "audio-music-rt: FAIL callback heap and mutex controls, cold overlap and concurrent stream");
    return ok ? 0 : 1;
}
