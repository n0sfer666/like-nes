#pragma once
#include <cstdint>

namespace framework::core {

constexpr uint8_t CREDITS_MAGIC[4] = {'L', 'N', 'C', 'R'};
constexpr uint32_t CREDITS_VERSION = 1;

struct CreditRow {
    uint32_t pack_offset;
    uint32_t author_offset;
    uint32_t license_offset;
    uint32_t url_offset;
    uint32_t attribution_offset;
};
static_assert(sizeof(CreditRow) == 20, "CreditRow layout pinned (zero-parse ABI)");

} // namespace framework::core
