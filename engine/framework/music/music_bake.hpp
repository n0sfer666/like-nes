#pragma once
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "music_format.hpp"

namespace framework::music {
struct MusicSrc {
    std::string name;
    uint64_t guid = 0;
    uint32_t frames = 0, rate = MUSIC_SAMPLE_RATE, channels = 2, loop_start = 0;
    std::optional<uint32_t> loop_end;
    uint32_t fade_ms = 1000;
};
// Метаданные из проверенного декодера. Ogg import/manifest — отдельный слой;
// fade_ms=0 означает мгновенную смену. Отказ сохраняет прежний out.
bool bake_music(std::span<const MusicSrc> tracks, std::vector<uint8_t>& out, std::string& error);
}
