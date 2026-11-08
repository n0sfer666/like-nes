#pragma once
#include "platform_net_discovery.hpp"

#include <string_view>

namespace platform::net {

class DiscoveryBackend {
public:
    virtual ~DiscoveryBackend() = default;
    virtual bool open(const DiscoveryConfig& config) = 0;
    virtual void poll(uint64_t now) = 0;

    bool good() const { return good_; }
    std::span<const DiscoveredRoom> rooms() const { return {rooms_.data(), count_}; }

protected:
    void clear();
    void add(std::string_view name, Endpoint endpoint, uint32_t scope);
    void remove(std::string_view name, uint32_t scope);
    void remove(std::string_view name, Endpoint endpoint, uint32_t scope);

    bool good_ = true;

private:
    std::array<DiscoveredRoom, DISCOVERY_ROOMS> rooms_{};
    std::array<uint32_t, DISCOVERY_ROOMS> scopes_{};
    size_t count_ = 0;
};

std::unique_ptr<DiscoveryBackend> make_discovery_backend();

} // namespace platform::net
