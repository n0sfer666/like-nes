#include "mixer.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {
constexpr uint32_t BLOCK_FRAMES = 64;
constexpr uint32_t BLOCKS = 4096;
constexpr uint32_t STREAM_VOICES = 3;
constexpr auto TIMEOUT = std::chrono::seconds(10);

bool run(audio::Backend backend) {
    using namespace audio;
    SampleRing ring;
    ring.init(1);
    Mixer mixer(backend);
    mixer.register_source(1, nullptr, 0, &ring);
    for (uint32_t i = 0; i < STREAM_VOICES; ++i) {
        AudioCommand command{};
        command.type = static_cast<uint32_t>(CmdType::Play);
        command.guid = 1;
        command.voice_id = i + 1;
        command.gain = fix32::from_int(1).raw;
        command.bus = static_cast<uint32_t>(Bus::Sfx);
        command.flags = CMD_FLAG_LOOP;
        if (!mixer.post(command)) return false;
    }

    std::atomic<bool> callback_ready{false};
    std::atomic<bool> start{false};
    std::atomic<bool> observer_saw_progress{false};
    std::atomic<bool> callback_done{false};
    std::atomic<bool> timed_out{false};
    const auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
    int16_t output[BLOCK_FRAMES * 2]{};
    std::thread callback([&] {
        callback_ready.store(true, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out.store(true, std::memory_order_relaxed);
                callback_done.store(true, std::memory_order_release);
                return;
            }
            std::this_thread::yield();
        }
        mixer.mix(BLOCK_FRAMES, output);
        while (!observer_saw_progress.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out.store(true, std::memory_order_relaxed);
                break;
            }
            std::this_thread::yield();
        }
        for (uint32_t block = 1; block < BLOCKS; ++block) {
            if (timed_out.load(std::memory_order_relaxed) ||
                std::chrono::steady_clock::now() >= deadline) {
                timed_out.store(true, std::memory_order_relaxed);
                break;
            }
            mixer.mix(BLOCK_FRAMES, output);
        }
        callback_done.store(true, std::memory_order_release);
    });

    std::thread observer([&] {
        while (!callback_ready.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out.store(true, std::memory_order_relaxed);
                return;
            }
            std::this_thread::yield();
        }
        uint64_t previous_underruns = 0;
        uint32_t previous_peak = 0;
        start.store(true, std::memory_order_release);
        while (!callback_done.load(std::memory_order_acquire)) {
            const uint64_t underruns = mixer.underruns();
            const uint32_t peak = mixer.peak_voices();
            if (underruns < previous_underruns || peak < previous_peak ||
                underruns > uint64_t{STREAM_VOICES} * BLOCK_FRAMES * BLOCKS ||
                peak > STREAM_VOICES) {
                timed_out.store(true, std::memory_order_relaxed);
                break;
            }
            previous_underruns = underruns;
            previous_peak = peak;
            if (underruns != 0) observer_saw_progress.store(true, std::memory_order_release);
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out.store(true, std::memory_order_relaxed);
                break;
            }
            std::this_thread::yield();
        }
    });

    callback.join();
    observer.join();
    const uint64_t expected = uint64_t{STREAM_VOICES} * BLOCK_FRAMES * BLOCKS;
    return !timed_out.load(std::memory_order_relaxed) && mixer.underruns() == expected &&
           mixer.peak_voices() == STREAM_VOICES;
}
} // namespace

int main() {
    const bool ok = run(audio::Backend::Float) && run(audio::Backend::Fix32);
    std::puts(ok ? "audio-mixer-stats: PASS concurrent Float/Fix32 underruns and peak voices" :
                   "audio-mixer-stats: FAIL");
    return ok ? 0 : 1;
}
