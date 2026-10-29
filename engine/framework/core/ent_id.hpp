#pragma once
#include <cstdint>

namespace framework::core {

struct EntId {
    uint32_t seq = 0;
};

constexpr bool operator==(EntId a, EntId b) { return a.seq == b.seq; }

} // namespace framework::core
