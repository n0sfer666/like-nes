#pragma once
#include <cstdint>

#include "brawl_input.hpp"

namespace rumble {

enum class Attack : uint8_t { None, Punch, Cross, Kick, Grab };

struct PlayerCommand {
    framework::brawl::BrawlInput input;
    Attack attack = Attack::None;
    bool operator==(const PlayerCommand&) const = default;
};

} // namespace rumble
