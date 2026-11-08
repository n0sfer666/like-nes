#pragma once
#include "net_relay_wire.hpp"

#include <optional>

namespace net {

enum class RelayPhase { Idle, Verifying, Ready, Refused, TimedOut, Failed };

class RelayClient {
public:
    explicit RelayClient(const platform::net::Endpoint& endpoint) : relay_(platform::net::normalized_endpoint(endpoint)) {}

    bool create(uint64_t now);
    bool join(RoomCode code, uint64_t now);
    bool retry_at(const platform::net::Endpoint& endpoint, uint64_t now);
    bool rebind(uint64_t now);
    std::optional<Datagram> poll(uint64_t now);
    void receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now);
    bool route(std::span<const uint8_t> payload, Datagram& out) const;
    std::span<const uint8_t> forward(const platform::net::Endpoint& from, std::span<const uint8_t> bytes) const;

    RelayPhase phase() const { return phase_; }
    RelayRefusal refusal() const { return refusal_; }
    RoomCode code() const { return code_; }
    uint64_t last_state() const { return last_state_; }
    const platform::net::Endpoint& peer() const { return peer_; }
    const platform::net::Endpoint& relay() const { return relay_; }

private:
    bool begin(RelayControl intent, uint64_t now);
    void verify_again(uint64_t now);
    void expire(uint64_t now);
    void verify(const RelayControl& f, uint64_t now);

    platform::net::Endpoint relay_;
    platform::net::Endpoint peer_;
    RelayControl pending_;
    RelayPhase phase_ = RelayPhase::Idle;
    RelayRefusal refusal_ = RelayRefusal::None;
    Token room_{};
    Token key_{};
    Token query_{};
    Token challenge_{};
    RoomCode code_ = 0;
    Role role_ = Role::Host;
    uint64_t deadline_ = 0;
    uint64_t next_ = 0;
    uint64_t last_state_ = 0;
    bool proof_next_ = false;
};

} // namespace net
