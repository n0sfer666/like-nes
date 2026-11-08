#pragma once
#include "net_peer_path.hpp"
#include "net_relay_client.hpp"

namespace rumble {

class NetworkChannel {
public:
    using Endpoint = platform::net::Endpoint;
    bool open(std::span<const Endpoint> endpoints, bool host, bool relay, net::RoomCode code,
              bool relay_only = false, uint64_t now = 0);
    void pump(uint64_t now);
    bool ready() const;
    bool failed() const;
    bool direct() const { return path_ && path_->direct(now_); }
    net::RoomCode code() const { return relay_ ? relay_->code() : 0; }
    const Endpoint& endpoint() const { return relay_ ? relay_->relay() : candidates_[0]; }
    void set_match(const net::SessionKey& key);

    int read(std::span<uint8_t> buffer, Endpoint& from);
    std::span<const uint8_t> logical(Endpoint& from, std::span<const uint8_t> bytes);
    void send(const Endpoint& to, const net::Datagram& packet, bool game);
    void close();

private:
    bool next_relay(uint64_t now);
    platform::net::Socket socket_;
    platform::net::Socket ipv6_;
    std::optional<net::RelayClient> relay_;
    std::optional<net::PeerPath> path_;
    uint64_t now_ = 0;
    std::array<Endpoint, 8> candidates_{};
    size_t count_ = 0;
    size_t next_ = 0;
    net::RoomCode join_code_ = 0;
    bool host_ = false;
    bool relay_only_ = false;
    bool alternate_ = false;
};

} // namespace rumble
