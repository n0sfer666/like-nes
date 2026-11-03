#pragma once
#include <span>

#include "audio_types.hpp"

namespace audio {
// Cold values: ровно один источник. Encoded/PCM bytes неизменны и живут до worker stop/join.
// PCM уже stereo/48000Hz; frames — длина полного источника, loop_end не включается.
// Первый проход начинается с0 (intro), следующие — сloop_start.
struct MusicTrack {
    uint64_t id = 0;
    std::span<const uint8_t> encoded;
    std::span<const StereoFrame> pcm;
    uint32_t frames = 0, loop_start = 0, loop_end = 0, fade_samples = SAMPLE_RATE;
};
}
