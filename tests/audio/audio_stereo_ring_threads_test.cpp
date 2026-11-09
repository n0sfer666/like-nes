#include "audio_stereo_ring.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace {
audio::StereoFrame sample(uint32_t i) {
    return {static_cast<int16_t>(static_cast<int32_t>(i % 251) - 125), static_cast<int16_t>(static_cast<int32_t>(i % 241) - 120)};
}
bool exact(audio::StereoFrame a, audio::StereoFrame b) { return a.left == b.left && a.right == b.right; }
}
int main() {
    using namespace audio;
    auto ring = std::make_unique<StereoRing>();
    constexpr uint32_t COUNT = 200000;
    std::atomic<bool> stop{false}, ready{false}, go{false};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    auto expired = [&] { return std::chrono::steady_clock::now() > deadline; };
    std::thread producer([&] {
        constexpr uint32_t BATCH_SIZE = 37;
        std::array<StereoFrame, BATCH_SIZE> batch;
        uint32_t i = 0;
        while (i < COUNT && !stop.load()) {
            const uint32_t n = std::min(BATCH_SIZE, COUNT - i);
            for (uint32_t j = 0; j < n; ++j) batch[j] = sample(i + j);
            i += ring->push(17, std::span(batch).first(n));
            if (expired()) { stop.store(true); break; }
        }
    });
    bool ok = true;
    for (uint32_t i = 0; i < COUNT && ok;) {
        StereoFrame frame;
        if (ring->take(17, frame)) { ok &= exact(frame, sample(i)); ++i; }
        if (expired() || stop.load()) ok = false;
    }
    stop.store(true); producer.join();
    // Новое поколение публикуется, пока producer ещё заканчивает запоздалый старый блок.
    constexpr uint64_t OLD = 1, NEW = (uint64_t{1} << 32) | 1;
    stop.store(false);
    std::thread delayed([&] {
        const StereoFrame old{900, -901};
        for (uint32_t i = 0; i < StereoRing::CAPACITY; ++i) if (ring->push(OLD, {&old, 1}) != 1) stop.store(true);
        ready.store(true, std::memory_order_release);
        while (!go.load(std::memory_order_acquire) && !stop.load()) if (expired()) stop.store(true);
        for (uint32_t i = 0; i < 32 && !stop.load(); ++i) ring->push(OLD, {&old, 1});
        for (uint32_t i = 0; i < 128 && !stop.load();) {
            const auto frame = sample(i);
            i += ring->push(NEW, {&frame, 1});
            if (expired()) stop.store(true);
        }
    });
    while (!ready.load(std::memory_order_acquire) && !expired()) std::this_thread::yield();
    StereoFrame frame;
    ok &= ready.load() && !stop.load() && !ring->take(NEW, frame) && exact(frame, {});
    go.store(true, std::memory_order_release);
    for (uint32_t i = 0; i < 128 && ok;) {
        if (ring->take(NEW, frame)) { ok &= exact(frame, sample(i)); ++i; }
        if (expired() || stop.load()) ok = false;
    }
    stop.store(true); delayed.join();
    std::puts(ok ? "audio-stereo-ring-threads: PASS concurrent FIFO and delayed old generation handoff" : "audio-stereo-ring-threads: FAIL");
    return ok ? 0 : 1;
}
