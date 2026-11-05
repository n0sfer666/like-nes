#pragma once
#include <cstdint>

namespace audio {
// Независимые атомарные показания. closed означает запрос остановки; worker может ещё завершаться.
struct MusicStats {
    uint64_t underruns = 0;
    uint64_t decode_failures = 0;
    uint32_t live_decoders = 0;
    uint32_t peak_decoders = 0;
    bool closed = true;
};
} // namespace audio
