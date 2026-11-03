#include "vorbis_reader.hpp"

#include <array>

namespace audio {
bool inspect_vorbis(std::span<const uint8_t> encoded, VorbisInfo& out, std::string& error) {
    out = {}; error.clear();
    VorbisReader reader;
    if (!reader.open(encoded, error)) return false;
    const auto info = reader.info();
    std::array<int16_t, 4096> block;
    uint64_t total = 0;
    for (;;) {
        uint32_t frames = 0;
        if (!reader.read(block, 2, frames)) { error = "Vorbis decode failed (error " + std::to_string(reader.last_error()) + ")"; return false; }
        if (!frames) break;
        total += frames;
        if (total > info.frames) { error = "Vorbis decoded length exceeds declared frames"; return false; }
    }
    if (total != info.frames) { error = "Vorbis declared length differs from decoded frames"; return false; }
    out = info; return true;
}
}
