#pragma once
#include <array>
#include <cstdint>

#include "brawl_body.hpp"

namespace framework::brawl {

constexpr uint32_t POOL_CAPACITY = 2 + 8 + 16;

struct BodyPool {
    std::array<Body, POOL_CAPACITY> bodies{};
    uint32_t count = 0;
    uint32_t next_seq = 1;

    EntId spawn(const Body& init);
    bool despawn(EntId id);
    Body* find(EntId id);
    const Body* find(EntId id) const;
};

} // namespace framework::brawl
