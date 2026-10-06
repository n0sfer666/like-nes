#pragma once
#include <cstdint>

#include "depth_body.hpp"
#include "ent_id.hpp"

namespace framework::brawl {

struct Body {
    EntId id;
    DepthBody pos;
    int8_t facing = 1;
    uint8_t team = 0;
    EntId owner;
    int32_t hp = 0;
    bool crushed = false;
    uint32_t age = 0;
};

} // namespace framework::brawl
