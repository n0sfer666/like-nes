#pragma once
#include <cstddef>
#include <cstdint>

namespace framework::music {
inline constexpr uint8_t MUSIC_MAGIC[4]{'L', 'N', 'M', 'U'};
inline constexpr uint32_t MUSIC_VERSION = 1, MUSIC_SAMPLE_RATE = 48000, MAX_MUSIC_TRACKS = 4096;
inline constexpr size_t MAX_MUSIC_NAME = 128, MAX_MUSIC_BYTES = 1u << 20;
struct MusicRow {
    uint64_t id, guid;
    uint32_t name, frames, loop_start, loop_end, fade_samples, rate, channels, reserved;
};
static_assert(sizeof(MusicRow) == 48);
}
