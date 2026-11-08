#include "platform_net_discovery_backend.hpp"

#include <algorithm>

namespace platform::net {

Discovery::Discovery() = default;
Discovery::~Discovery() = default;

bool Discovery::open(const DiscoveryConfig& config) {
    close();
    if (config.port && (config.name.empty() || config.name.size() > 63 ||
        config.name.find('\0') != std::string::npos)) return false;
    backend_ = make_discovery_backend();
    if (!backend_ || !backend_->open(config)) {
        close();
        return false;
    }
    return true;
}

void Discovery::close() { backend_.reset(); }
void Discovery::poll(uint64_t now) { if (backend_) backend_->poll(now); }
bool Discovery::available() const { return backend_ && backend_->good(); }
std::span<const DiscoveredRoom> Discovery::rooms() const {
    return available() ? backend_->rooms() : std::span<const DiscoveredRoom>{};
}

void DiscoveryBackend::clear() {
    while (count_) rooms_[--count_] = {};
}

void DiscoveryBackend::add(std::string_view name, Endpoint endpoint, uint32_t scope) {
    endpoint = normalized_endpoint(endpoint);
    if (name.empty() || name.size() > 63 || !endpoint.port || count_ == rooms_.size()) return;
    for (size_t i = 0; i < count_; ++i) {
        if (rooms_[i].name == name && rooms_[i].endpoint == endpoint && scopes_[i] == scope) return;
    }
    rooms_[count_] = {std::string(name), endpoint};
    scopes_[count_] = scope;
    ++count_;
}

void DiscoveryBackend::remove(std::string_view name, uint32_t scope) {
    for (size_t i = 0; i < count_;) {
        if (rooms_[i].name != name || scopes_[i] != scope) {
            ++i;
            continue;
        }
        remove(name, rooms_[i].endpoint, scope);
    }
}

void DiscoveryBackend::remove(std::string_view name, Endpoint endpoint, uint32_t scope) {
    for (size_t i = 0; i < count_; ++i) {
        if (rooms_[i].name != name || scopes_[i] != scope || rooms_[i].endpoint != endpoint) continue;
        for (size_t j = i + 1; j < count_; ++j) {
            rooms_[j - 1] = std::move(rooms_[j]);
            scopes_[j - 1] = scopes_[j];
        }
        --count_;
        rooms_[count_] = {};
        return;
    }
}

} // namespace platform::net
