#pragma once
#include <cstdint>
#include <optional>

namespace audio {
// Наблюдатель, вне rollback snapshot. Порог передаёт caller из rollback::settle_ticks(depth).
// observe — одно законченное наблюдение тика и желания: replay не меняет даже candidate.
// Один owning thread и непрерывная эпоха тиков; после Session::reset создаётся новый intent.
class MusicIntent {
public:
    explicit MusicIntent(uint32_t settle_ticks) : threshold_(settle_ticks) {}
    bool valid() const { return threshold_ != 0; }
    std::optional<uint64_t> stable_track() const { return selected_; }
    std::optional<uint64_t> observe(uint32_t tick, uint64_t track) {
        if (!valid()) return {};
        if (seen_tick_) {
            const uint32_t delta = tick - last_tick_;
            // Модульный high-water: непрерывный UINT32_MAX→0 разрешён, old/duplicate — нет.
            if (!delta || delta >= (uint32_t{1} << 31)) return {};
            if (delta != 1) count_ = 0; // непроверенные тики не считаются устойчивостью
        }
        seen_tick_ = true; last_tick_ = tick;
        if (!count_ || candidate_ != track) { candidate_ = track; count_ = 1; }
        else if (count_ < threshold_) ++count_;
        if (count_ == threshold_ && (!selected_ || *selected_ != candidate_)) {
            selected_ = candidate_; return selected_;
        }
        return {};
    }
private:
    uint32_t threshold_ = 0, last_tick_ = 0, count_ = 0;
    uint64_t candidate_ = 0;
    std::optional<uint64_t> selected_;
    bool seen_tick_ = false;
};
}
