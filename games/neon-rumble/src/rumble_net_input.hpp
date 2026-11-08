#pragma once
#include "rumble_net_sim.hpp"

namespace rumble {

constexpr uint32_t NETWORK_INPUTS = NETWORK_DEPTH + 4;

struct NetworkInputPacket {
    uint32_t begin = 0;
    uint32_t confirmed = 0; // Exclusive count of contiguous ticks with both inputs.
    uint32_t hash_tick = 0;
    std::array<uint64_t, NETWORK_INPUTS> hashes{};
    uint32_t probe = 0;
    uint32_t echo = 0;
    std::array<PlayerCommand, NETWORK_INPUTS> inputs{};
    uint8_t count = 0;
    uint8_t hash_count = 0;
};

size_t encode_network_input(const NetworkInputPacket& packet, std::span<uint8_t> bytes);
bool decode_network_input(std::span<const uint8_t> bytes, NetworkInputPacket& out);

} // namespace rumble
