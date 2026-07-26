#pragma once
#include <cstdint>
#include <vector>

#include "format.hpp"

// Детерминированный сериализатор бандла (гейт #1: байт-в-байт репродьюсибельно).
// Правила детерминизма: фикс. порядок (сорт по guid), без таймштампов/путей,
// стабильное выравнивание. Тот же вход → тот же байт-выход.
namespace asset {

struct AssetInput {
    uint64_t guid;
    AssetType type;
    Codec codec;
    Residency residency;
    std::vector<uint8_t> payload; // на диске (уже сжато кодеком)
    uint32_t uncompressed_size;
    uint32_t tex_w = 0;
    uint32_t tex_h = 0;
    uint32_t tex_format = 0;
    uint32_t variant_key = 0;
};

// Собирает бандл детерминированно. Возвращает байты .bundle.
std::vector<uint8_t> write_bundle(std::vector<AssetInput> assets);

} // namespace asset
