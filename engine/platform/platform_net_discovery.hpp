#pragma once
#include "platform_net.hpp"

#include <memory>

namespace platform::net {

inline constexpr char DISCOVERY_SERVICE[] = "_likenes._udp";
inline constexpr size_t DISCOVERY_ROOMS = 16;

struct DiscoveredRoom {
    std::string name;
    Endpoint endpoint;
};

struct DiscoveryConfig {
    std::string name;
    uint16_t port = 0; // Zero browses. A nonzero port advertises this service.
    bool local_only = false;
};

class DiscoveryBackend;

class Discovery {
public:
    Discovery();
    ~Discovery();
    Discovery(const Discovery&) = delete;
    Discovery& operator=(const Discovery&) = delete;

    bool open(const DiscoveryConfig& config);
    void close();
    void poll(uint64_t monotonic_ms);
    bool available() const;
    std::span<const DiscoveredRoom> rooms() const;

private:
    std::unique_ptr<DiscoveryBackend> backend_;
};

} // namespace platform::net
