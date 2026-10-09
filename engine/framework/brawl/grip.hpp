#pragma once
#include <cstdint>

#include "ent_id.hpp"

namespace framework::brawl {

struct Grip {
    EntId by;
    uint8_t team = 0;
    uint8_t kind = 0;
};

} // namespace framework::brawl
