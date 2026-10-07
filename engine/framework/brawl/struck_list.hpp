#pragma once
#include <array>
#include <cstdint>

#include "ent_id.hpp"

namespace framework::brawl {

constexpr uint32_t MAX_STRUCK = 16;

struct Struck {
    uint32_t seq = 0;
    uint32_t box = 0;
};

struct StruckList {
    std::array<Struck, MAX_STRUCK> at{};
    uint32_t count = 0;

    bool has(EntId target, uint8_t box) const;
    bool add(EntId target, uint8_t box);
    void forget(uint8_t box);
};

} // namespace framework::brawl
