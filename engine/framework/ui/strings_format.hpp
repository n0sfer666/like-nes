#pragma once
#include <cstddef>
#include <cstdint>

namespace framework::ui {
inline constexpr uint8_t STRINGS_MAGIC[4]{'L', 'N', 'S', 'T'};
inline constexpr uint32_t STRINGS_VERSION = 1;
inline constexpr uint32_t MAX_STRINGS = 4096;
inline constexpr size_t MAX_STRINGS_BYTES = 16u << 20;
inline constexpr size_t MAX_STRING_KEY = 128;
enum class Language : uint8_t { En, Ru };
struct StringRow {
    uint64_t id;
    uint32_t key, en, ru, fallback, en_size, ru_size;
};
struct StringsMeta {
    uint32_t font, fallbacks, reserved0, reserved1;
};
static_assert(sizeof(StringRow) == 32 && sizeof(StringsMeta) == 16);
}
