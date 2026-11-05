#include "music_control.hpp"

#include <algorithm>
#include <utility>

namespace audio {
bool MusicControl::init(std::span<const MusicTrack> tracks, uint32_t settle_ticks, std::string& error) {
    error.clear();
    if (initialized_ || tracks.empty() || tracks.size() > 4096 || !settle_ticks) {
        error = "music control needs1..4096 tracks and a nonzero settle threshold";
        return false;
    }
    std::vector<Entry> entries;
    entries.reserve(tracks.size());
    for (uint32_t i = 0; i < tracks.size(); ++i)
        entries.push_back({tracks[i].id, i, tracks[i].fade_samples});
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.id < b.id; });
    for (size_t i = 1; i < entries.size(); ++i) {
        if (entries[i - 1].id == entries[i].id) {
            error = "music control track ids must be unique";
            return false;
        }
    }
    settle_ticks_ = settle_ticks;
    intent_ = MusicIntent(settle_ticks);
    catalogue_ = std::move(entries);
    initialized_ = true;
    return true;
}

bool MusicControl::want(uint64_t track_id) {
    if (!initialized_) return false;
    const auto found = std::lower_bound(catalogue_.begin(), catalogue_.end(), track_id,
        [](const Entry& entry, uint64_t id) { return entry.id < id; });
    if (found == catalogue_.end() || found->id != track_id) return false;
    latest_ = *found;
    return true;
}

void MusicControl::tick(uint32_t tick_value, MusicRuntime& runtime) {
    if (!initialized_ || runtime.transport().closed.load(std::memory_order_acquire)) return;
    if (latest_) {
        if (const auto stable = intent_.observe(tick_value, latest_->id))
            pending_ = MusicWish{latest_->index, latest_->fade_samples};
    }
    if (pending_ && runtime.post(*pending_)) pending_.reset();
}

void MusicControl::reset_intent() {
    if (!initialized_) return;
    intent_ = MusicIntent(settle_ticks_);
    latest_.reset();
    pending_.reset();
}
} // namespace audio
