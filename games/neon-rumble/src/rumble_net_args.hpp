#pragma once
#include "net_relay_wire.hpp"

#include <string_view>

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
size_t resolve_relay_address(std::string_view text, std::span<platform::net::Endpoint> endpoints);
bool network_bundle_hash(const std::string& path, uint64_t& hash);

} // namespace rumble
