#pragma once
#include <cstdint>

namespace framework::core {

struct SectionHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t count;
    uint32_t rows_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(SectionHeader) == 24, "SectionHeader layout pinned (zero-parse ABI)");

} // namespace framework::core
