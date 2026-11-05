#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "music_intent.hpp"
#include "music_runtime.hpp"
#include "music_track.hpp"

namespace audio {
// Таблица выбора, Intent и pending — один control thread. RT вызывает только MusicRuntime::next.
class MusicControl {
public:
    bool init(std::span<const MusicTrack> tracks, uint32_t settle_ticks, std::string& error);
    bool want(uint64_t track_id);
    void tick(uint32_t tick, MusicRuntime& runtime);
    void reset_intent();

private:
    struct Entry { uint64_t id; uint32_t index, fade_samples; };
    uint32_t settle_ticks_ = 0;
    MusicIntent intent_{1};
    std::vector<Entry> catalogue_;
    std::optional<Entry> latest_;
    std::optional<MusicWish> pending_;
    bool initialized_ = false;
};
} // namespace audio
