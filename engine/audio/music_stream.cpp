#include "music_stream.hpp"

#include <chrono>
#include <system_error>
#include <thread>
#include <utility>

#include "mixer.hpp"
#include "music_control.hpp"
#include "music_runtime.hpp"
#include "music_worker.hpp"

namespace audio {
struct MusicStream::Impl {
    MusicControl control;
    std::shared_ptr<MusicRuntime> runtime;
    std::unique_ptr<MusicWorker> worker;
    std::thread worker_thread;
    bool initialized = false;

    void stop() {
        if (runtime) runtime->transport().closed.store(true, std::memory_order_release);
        if (worker_thread.joinable()) worker_thread.join();
        worker.reset();
    }
    ~Impl() { stop(); }
};

MusicStream::MusicStream() : impl_(std::make_unique<Impl>()) {}
MusicStream::~MusicStream() = default;

bool MusicStream::init(Mixer& mixer, std::span<const MusicTrack> tracks, uint32_t settle_ticks,
                       std::string& error) {
    error.clear();
    if (impl_->initialized) { error = "music stream already initialized"; return false; }
    MusicControl control;
    if (!control.init(tracks, settle_ticks, error)) return false;

    auto runtime = std::make_shared<MusicRuntime>(static_cast<uint32_t>(tracks.size()));
    auto worker = std::make_unique<MusicWorker>(runtime->transport());
    if (!worker->init(tracks, error)) return false;
    std::thread worker_thread;
    try {
        MusicWorker* const pump = worker.get();
        worker_thread = std::thread([pump] {
            while (pump->pump()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
    } catch (const std::system_error& exception) {
        runtime->transport().closed.store(true, std::memory_order_release);
        error = exception.what();
        return false;
    }
    if (!MusicRuntime::bind(mixer, runtime)) {
        runtime->transport().closed.store(true, std::memory_order_release);
        worker_thread.join();
        error = "music runtime could not bind to this mixer";
        return false;
    }

    impl_->control = std::move(control);
    impl_->runtime = std::move(runtime);
    impl_->worker = std::move(worker);
    impl_->worker_thread = std::move(worker_thread);
    impl_->initialized = true;
    return true;
}

bool MusicStream::want(uint64_t track_id) {
    if (!impl_->initialized || impl_->runtime->transport().closed.load(std::memory_order_acquire)) return false;
    return impl_->control.want(track_id);
}

void MusicStream::tick(uint32_t tick) {
    if (!impl_->initialized || impl_->runtime->transport().closed.load(std::memory_order_acquire)) return;
    impl_->control.tick(tick, *impl_->runtime);
}

void MusicStream::reset_intent() {
    if (!impl_->initialized) return;
    impl_->control.reset_intent();
}

MusicStats MusicStream::stats() const {
    return impl_->runtime ? impl_->runtime->stats() : MusicStats{};
}
} // namespace audio
