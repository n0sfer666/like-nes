#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "frame_stats.hpp"
#include "rumble_frame_music.hpp"

namespace rumble {

using FrameClock = std::chrono::steady_clock;
enum class FrameStatsSource { Desktop, Mobile, Fixture };

class FrameStatsRun {
public:
    static constexpr uint32_t WARMUP_FRAMES = 60;
    static constexpr uint32_t MENU_FRAMES = 120;
    static constexpr uint32_t CAPTURE_FRAMES = 3600;

    bool open(const std::string& bundle, FrameStatsSource source, std::string& why,
              uint32_t capture_frames = CAPTURE_FRAMES);
    bool collecting() const { return opened_ && !ready() && !reported_; }
    bool ready() const { return opened_ && frames_.size() == capacity_; }
    uint32_t presented() const { return presented_; }
    // Call only after upload and submission for presentation. A failed frame does not advance
    // the cadence baseline; deliberate lifecycle interruptions explicitly clear it.
    void present(FrameClock::time_point begin, FrameClock::time_point end, bool menu);
    void reset_cadence() { previous_.reset(); }
    void observe_touch(bool move, bool attack);
    void tick();
    void poll();
    void suspend();
    void resume();
    // On mobile, allow_save is true only from Game::save(). Emits at most one report.
    bool finish(bool allow_save);

private:
    framework::core::FrameStats frames_;
    FrameMusic music_;
    FrameMusicStats audio_start_, audio_end_;
    std::optional<FrameClock::time_point> previous_;
    FrameStatsSource source_ = FrameStatsSource::Fixture;
    uint32_t capacity_ = 0, presented_ = 0, menu_frames_ = 0;
    bool moved_ = false, attacked_ = false, valid_ = true, opened_ = false;
    bool reported_ = false, complete_ = false;
};

} // namespace rumble
