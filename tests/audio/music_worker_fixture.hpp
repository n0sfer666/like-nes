#pragma once
#include <array>

#include "music_track.hpp"

namespace music_worker_fixture {
inline constexpr std::array<audio::StereoFrame, 5> FIRST{{{100, -200}, {300, -400}, {500, -600}, {700, -800}, {900, -1000}}};
inline constexpr std::array<audio::StereoFrame, 4> SECOND{{{-110, 220}, {-330, 440}, {-550, 660}, {-770, 880}}};
inline std::array<audio::MusicTrack, 2> tracks() {
    audio::MusicTrack a, b;
    a.id = 0; a.pcm = FIRST; a.frames = 5; a.loop_start = 1; a.loop_end = 4;
    b.id = UINT64_MAX; b.pcm = SECOND; b.frames = 4; b.loop_start = 2; b.loop_end = 4;
    return {a, b};
}
inline audio::StereoFrame at(const audio::MusicTrack& track, uint32_t frame) {
    const uint32_t index = frame < track.loop_end ? frame : track.loop_start + (frame - track.loop_end) % (track.loop_end - track.loop_start);
    return track.pcm[index];
}
inline bool exact(audio::StereoFrame a, audio::StereoFrame b) { return a.left == b.left && a.right == b.right; }
}
