#pragma once
#include "net_relay_wire.hpp"

namespace rumble {

struct NetworkRun {
    std::array<platform::net::Endpoint, 8> endpoints{};
    size_t endpoint_count = 0;
    uint32_t frames = 0;
    net::RoomCode code = 0;
    bool host = false;
    bool relay = false;
    bool relay_only = false;
};

bool parse_network_run(int argc, char** argv, NetworkRun& out);

} // namespace rumble
