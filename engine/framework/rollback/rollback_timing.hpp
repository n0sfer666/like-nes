#pragma once
#include <cstdint>

namespace framework::rollback {
inline constexpr uint32_t LEAD = 16;
// Ноль означает переполнение конфигурации, а не отсутствие задержки.
constexpr uint32_t settle_ticks(uint32_t depth) {
    return depth <= UINT32_MAX - LEAD - 1 ? depth + LEAD + 1 : 0;
}
template <uint32_t Depth>
inline constexpr uint32_t SETTLE_TICKS = [] {
    static_assert(Depth <= UINT32_MAX - LEAD - 1, "rollback settling depth overflows");
    return settle_ticks(Depth);
}();
}
