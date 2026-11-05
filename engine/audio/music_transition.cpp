#include "music_runtime.hpp"

namespace audio {
namespace {
constexpr uint32_t THIRD_FADE = SAMPLE_RATE / 100;
}

void MusicRuntime::handle(MusicWish wish) {
    int32_t priming = -1, matching = -1;
    uint32_t playing = 0;
    for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) {
        const Slot& slot = slots_[i];
        if (slot.phase == Phase::Priming) priming = static_cast<int32_t>(i);
        if (slot.phase == Phase::Playing) {
            ++playing;
            if (slot.track == wish.track) matching = static_cast<int32_t>(i);
        }
    }

    if (release_slot_ >= 0) {
        const uint32_t release = static_cast<uint32_t>(release_slot_);
        const uint32_t held = release ^ 1u;
        if (slots_[release].track == wish.track) {
            deferred_ = false;
            ramp(slots_[release], fix32::ONE, wish.fade_samples);
            ramp(slots_[held], 0, wish.fade_samples);
            release_slot_ = -1;
        } else {
            deferred_ = true;
            deferred_track_ = wish.track;
            deferred_fade_ = wish.fade_samples;
            if (slots_[held].phase == Phase::Playing) hold(slots_[held]);
        }
        return;
    }

    if (priming >= 0 && slots_[static_cast<uint32_t>(priming)].track == wish.track) return;
    if (matching >= 0) {
        const uint32_t target = static_cast<uint32_t>(matching);
        Slot& selected = slots_[target];
        if (priming >= 0) retire(static_cast<uint32_t>(priming));
        if (playing == 2) {
            if (selected.direction != Gain::Falling) return; // повтор incoming не перезапускает рампу
            const uint32_t other = target ^ 1u;
            ramp(selected, fix32::ONE, wish.fade_samples);
            ramp(slots_[other], 0, wish.fade_samples);
        } else {
            if (priming < 0 && selected.direction == Gain::Rising) return;
            if (selected.gain.done() && selected.gain.value() == fix32::ONE) return;
            ramp(selected, fix32::ONE, wish.fade_samples);
        }
        return;
    }

    if (priming >= 0) {
        begin_load(static_cast<uint32_t>(priming), wish);
        return;
    }
    if (playing == 2) {
        const uint32_t release = slots_[0].direction == Gain::Falling ? 0u : 1u;
        const uint32_t held = release ^ 1u;
        release_slot_ = static_cast<int32_t>(release);
        ramp(slots_[release], 0, THIRD_FADE);
        hold(slots_[held]);
        deferred_ = true;
        deferred_track_ = wish.track;
        deferred_fade_ = wish.fade_samples;
        return;
    }
    if (playing == 1) {
        const uint32_t empty = slots_[0].phase == Phase::Empty ? 0u : 1u;
        hold(slots_[empty ^ 1u]);
        begin_load(empty, wish);
        return;
    }
    begin_load(0, wish);
}

void MusicRuntime::update() {
    if (!valid_ || transport_.closed.load(std::memory_order_acquire)) return;
    int32_t freed = -1;
    for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) {
        Slot& slot = slots_[i];
        const uint64_t failed = transport_.slots[i].failed.load(std::memory_order_acquire);
        if (slot.phase != Phase::Empty && failed == slot.generation) {
            const bool was_releasing = release_slot_ >= 0;
            retire(i);
            // Отказ held-слота также отменяет короткий спад: pending cue занимает освободившийся.
            release_slot_ = -1;
            freed = static_cast<int32_t>(i);
            for (uint32_t j = 0; j < MUSIC_SLOTS; ++j) {
                if (j != i && slots_[j].phase == Phase::Playing &&
                    (was_releasing || slots_[j].direction == Gain::Falling)) hold(slots_[j]);
            }
        } else if (slot.phase == Phase::Playing && slot.direction == Gain::Falling &&
                   slot.gain.done() && slot.gain.value() == 0) {
            // Held тоже бывает нулевым из-за Q16 rounding, но обязан сохранять decoder/cursor.
            retire(i);
            if (release_slot_ == static_cast<int32_t>(i)) release_slot_ = -1;
            freed = static_cast<int32_t>(i);
        }
    }

    MusicWish wish;
    bool have_wish = false;
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP; ++i) {
        MusicWish candidate;
        if (!wishes_.pop(candidate)) break;
        wish = candidate;
        have_wish = true;
    }
    if (have_wish && freed >= 0 && deferred_ && release_slot_ < 0) deferred_ = false;
    if (have_wish) handle(wish);
    if (freed >= 0 && deferred_ && release_slot_ < 0)
        start_deferred(static_cast<uint32_t>(freed));
    retry_requests();

    for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) {
        Slot& slot = slots_[i];
        if (slot.phase != Phase::Priming) continue;
        auto& buffer = transport_.slots[i];
        if (!slot.has_primer) slot.has_primer = buffer.pcm.take(slot.generation, slot.primer);
        if (buffer.ready.load(std::memory_order_acquire) != slot.generation) continue;
        slot.phase = Phase::Playing;
        ramp(slot, fix32::ONE, slot.fade_samples);
        for (uint32_t j = 0; j < MUSIC_SLOTS; ++j) {
            if (j == i || slots_[j].phase != Phase::Playing) continue;
            ramp(slots_[j], 0, slot.fade_samples);
        }
    }
}
} // namespace audio
