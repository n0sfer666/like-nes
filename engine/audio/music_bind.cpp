#include "mixer.hpp"
#include "music_runtime.hpp"

namespace audio {
bool MusicRuntime::bind(Mixer& mixer, const std::shared_ptr<MusicRuntime>& runtime) {
    if (!runtime || !runtime->valid_ || runtime->transport_.closed.load(std::memory_order_acquire)) return false;
    if (!runtime->claim_binding()) return false;
    bool expected = false;
    if (!mixer.music_bound_once_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        runtime->bound_once_.store(false, std::memory_order_release);
        return false;
    }
    if (runtime->transport_.closed.load(std::memory_order_acquire)) {
        mixer.music_bound_once_.store(false, std::memory_order_release);
        runtime->bound_once_.store(false, std::memory_order_release);
        return false;
    }
    mixer.music_runtime_pin_ = runtime;
    mixer.music_runtime_ = runtime.get();
    return true;
}
} // namespace audio
