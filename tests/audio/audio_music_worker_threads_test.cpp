#include "music_worker.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "music_worker_fixture.hpp"

int main() {
    using namespace audio;
    auto t = std::make_unique<MusicTransport>(); MusicWorker worker(*t); std::string error;
    const auto tracks = music_worker_fixture::tracks();
    if (!worker.init(tracks, error)) return 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    std::atomic<bool> failed{false};
    std::thread decoder([&] {
        while (worker.pump()) {
            if (std::chrono::steady_clock::now() > deadline) { failed.store(true); t->closed.store(true); break; }
            std::this_thread::yield();
        }
    });
    bool ok = true;
    for (uint64_t generation = 1; generation < 35 && ok; ++generation) {
        const uint32_t index = static_cast<uint32_t>(generation % 2);
        // RT producer устанавливает expected tag до публикации; consumer тот же поток.
        while (!t->request(0, index, generation) && !failed.load() && std::chrono::steady_clock::now() <= deadline) std::this_thread::yield();
        bool primer = false; StereoFrame first;
        while (t->slots[0].ready.load(std::memory_order_acquire) != generation && !failed.load() && std::chrono::steady_clock::now() <= deadline) {
            if (!primer) primer = t->slots[0].pcm.take(generation, first);
            std::this_thread::yield();
        }
        ok &= !failed.load() && std::chrono::steady_clock::now() <= deadline;
        uint32_t cursor = 0;
        if (primer) { ok &= music_worker_fixture::exact(first, music_worker_fixture::at(tracks[index], cursor)); ++cursor; }
        while (cursor < 2000 && ok) {
            StereoFrame frame;
            if (t->slots[0].pcm.take(generation, frame)) {
                ok &= music_worker_fixture::exact(frame, music_worker_fixture::at(tracks[index], cursor)); ++cursor;
            } else if (std::chrono::steady_clock::now() > deadline || failed.load()) ok = false;
        }
        ok &= t->peak_decoders.load() <= 2;
    }
    // Закрываем и при заполненном PCM, и при оставшихся командах; аудио ack не нужен.
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP; ++i) t->request(i % 2, i % 2, 100 + i);
    t->closed.store(true, std::memory_order_release); decoder.join();
    ok &= !failed.load() && !t->live_decoders.load() && !t->failures.load();
    std::puts(ok ? "audio-music-worker-threads: PASS concurrent requests, PCM generations, priming and stop without callback ack" : "audio-music-worker-threads: FAIL");
    return ok ? 0 : 1;
}
