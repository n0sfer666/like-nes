#pragma once
#include <cstdint>

#include "depth_body.hpp"
#include "ent_id.hpp"
#include "reaction.hpp"
#include "run_state.hpp"
#include "strike_queue.hpp"
#include "struck_list.hpp"

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
    uint8_t kind = 0;
    uint16_t clip = 0;
    uint16_t move = NO_STRIKE;
    uint32_t elapsed = 0;
    uint16_t hitstop = 0;
    Reaction react = Reaction::None;
    uint16_t react_ticks = 0;
    StruckList struck;
    uint8_t chain = NO_CHAIN;
    StrikeQueue queued;
    RunState run;
};

} // namespace framework::brawl
