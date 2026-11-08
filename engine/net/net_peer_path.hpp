#pragma once
#include "net_session_wire.hpp"

#include <optional>

namespace net {

constexpr uint64_t PATH_PROBE_MS = 200;
constexpr uint64_t PATH_LOST_MS = 750;

class PeerPath {
public:
    explicit PeerPath(SessionKey key) : key_(key) {}
    void set_candidate(const platform::net::Endpoint& peer);
    const platform::net::Endpoint& candidate() const { return candidate_; }
    bool direct(uint64_t now) const;
    std::optional<Datagram> poll(uint64_t now);
    std::optional<Datagram> receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now);
    bool accepts(const platform::net::Endpoint& from, std::span<const uint8_t> bytes) const;

private:
    enum class Kind : uint8_t { Probe = 1, Echo = 2 };

    std::optional<Datagram> packet(Kind kind, uint64_t serial) const;
    bool open(std::span<const uint8_t> bytes, Kind& kind, uint64_t& serial) const;

    SessionKey key_;
    platform::net::Endpoint candidate_;
    std::optional<uint64_t> heard_;
    std::optional<uint64_t> verified_;
    uint64_t next_ = 0;
    uint64_t sent_at_ = 0;
    uint64_t serial_ = 0;
    uint64_t peer_serial_ = 0;
    bool pending_ = false;
};

} // namespace net
