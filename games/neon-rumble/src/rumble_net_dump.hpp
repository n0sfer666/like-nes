#pragma once
#include "rumble_net_fight.hpp"

namespace rumble {

constexpr uint32_t NETWORK_DUMP_VERSION = 1;
// Stem is a local path without extension. The snapshot is the pair's completion marker.
bool write_network_dump(const NetworkFight& fight, const std::string& stem);

class NetworkDump {
public:
    bool write(const NetworkFight& fight, uint64_t now);

private:
    std::string stem_;
    std::optional<uint32_t> saved_;
    uint64_t next_ = 0;
};

} // namespace rumble
