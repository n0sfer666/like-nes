#pragma once
#include "platform_net.hpp"

#include <string_view>

namespace platform::net::mdns {

inline constexpr char SERVICE[] = "_likenes._udp.local";
inline constexpr Endpoint MULTICAST{0xe00000fbu, 5353};
constexpr uint16_t A = 1, PTR = 12, TXT = 16, AAAA = 28, SRV = 33, ANY = 255;

struct Record {
    std::string name;
    std::string target;
    Endpoint address;
    uint32_t ttl = 0;
    uint16_t type = 0;
    uint16_t port = 0;
};

struct Message {
    bool response = false;
    std::array<Record, 32> records{};
    size_t count = 0;
};

struct Packet {
    std::array<uint8_t, MAX_DATAGRAM> bytes{};
    size_t size = 0;
};

bool parse(std::span<const uint8_t> bytes, Message& out);
Packet query(std::string_view name, uint16_t type);
Packet announce(std::string_view instance, std::string_view host, Endpoint address, uint32_t ttl);
std::string_view service_instance(std::string_view name);

} // namespace platform::net::mdns
