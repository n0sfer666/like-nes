#include "music_runtime.hpp"

namespace audio {
MusicRuntime::MusicRuntime(uint32_t track_count)
    : track_count_(track_count), valid_(track_count >= 1 && track_count <= 4096) {}

void MusicRuntime::hold(Slot& slot) {
    slot.gain.hold(); slot.direction = Gain::Held;
}

void MusicRuntime::ramp(Slot& slot, int32_t target, uint32_t frames) {
    // Внутренние цели только0/ONE: направление отличает повтор входящего от возврата уходящего.
    slot.gain.ramp_to(target, frames);
    slot.direction = target == 0 ? Gain::Falling : Gain::Rising;
}

bool MusicRuntime::post(MusicWish wish) {
    if (!valid_ || wish.track >= track_count_ || transport_.closed.load(std::memory_order_acquire)) return false;
    return wishes_.push(wish);
}

bool MusicRuntime::claim_binding() {
    bool expected = false;
    return bound_once_.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
}

void MusicRuntime::record_underruns(uint64_t count) {
    if (count) mix_underruns_.fetch_add(count, std::memory_order_relaxed);
}

MusicStats MusicRuntime::stats() const {
    MusicStats result;
    result.underruns = mix_underruns_.load(std::memory_order_relaxed);
    result.decode_failures = transport_.failures.load(std::memory_order_relaxed);
    result.live_decoders = transport_.live_decoders.load(std::memory_order_relaxed);
    result.peak_decoders = transport_.peak_decoders.load(std::memory_order_relaxed);
    result.closed = transport_.closed.load(std::memory_order_acquire);
    return result;
}

bool MusicRuntime::new_generation(uint64_t& generation) {
    if (generation_ == UINT64_MAX) {
        transport_.closed.store(true, std::memory_order_release);
        return false;
    }
    generation = ++generation_;
    return true;
}

void MusicRuntime::begin_load(uint32_t index, MusicWish wish) {
    uint64_t generation = 0;
    if (!new_generation(generation)) return;
    Slot& slot = slots_[index];
    slot = Slot{};
    slot.phase = Phase::Priming;
    slot.track = wish.track;
    slot.fade_samples = wish.fade_samples;
    slot.generation = generation;
    pending_requests_[index] = {{generation, index, wish.track}, true};
    retry_requests();
}

void MusicRuntime::retire(uint32_t index) {
    Slot& slot = slots_[index];
    if (slot.phase == Phase::Empty) return;
    uint64_t generation = 0;
    if (!new_generation(generation)) return;
    slot = Slot{};
    pending_requests_[index] = {{generation, index, MUSIC_STOP_TRACK}, true};
}

void MusicRuntime::retry_requests() {
    for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) {
        auto& pending = pending_requests_[i];
        if (pending.active && transport_.request(pending.load.slot, pending.load.track, pending.load.generation))
            pending.active = false;
    }
}

void MusicRuntime::start_deferred(uint32_t index) {
    if (!deferred_) return;
    const MusicWish wish{deferred_track_, deferred_fade_};
    deferred_ = false;
    for (auto& slot : slots_) {
        if (slot.phase == Phase::Playing && slot.track == wish.track) {
            ramp(slot, fix32::ONE, wish.fade_samples);
            return;
        }
    }
    begin_load(index, wish);
}
} // namespace audio
