#pragma once
#include <cstdint>
#include <vector>

// Декод Ogg Vorbis → mono int16 (спека #3, кодек-класс opus/vorbis спеки #5). Реальный
// декодер stb_vorbis (present в fetched stb). Живёт на decode-worker (вне audio-callback и sim).
namespace audio {

struct DecodedPcm {
    std::vector<int16_t> samples; // mono int16
    uint32_t frames = 0;
    uint32_t rate = 0;
};

// false при битом/невалидном контейнере → вызывающий ставит тишину-placeholder (не crash).
bool decode_vorbis(const uint8_t* data, uint32_t size, DecodedPcm& out);

} // namespace audio
