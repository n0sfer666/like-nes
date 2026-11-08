#include "rumble_net_channel.hpp"

#include <algorithm>

namespace rumble {
namespace {

constexpr uint64_t RELAY_STALE_MS = 2'000;

} // namespace

bool NetworkChannel::open(std::span<const Endpoint> endpoints, bool host, bool relay,
                          net::RoomCode code, bool relay_only, uint64_t now) {
    if (endpoints.empty() || endpoints.size() > candidates_.size()) return false;
    close();
    std::ranges::copy(endpoints, candidates_.begin());
    count_ = endpoints.size();
    host_ = host;
    join_code_ = code;
    relay_only_ = relay_only;
    if (relay) return next_relay(now);
    Endpoint local;
    local.family = candidates_[0].family;
    if (!socket_.open(host ? candidates_[0] : local)) return false;
    if (host && candidates_[0].family == platform::net::AddressFamily::IPv4 &&
        candidates_[0].address == platform::net::ADDRESS_ANY) {
        Endpoint dual;
        dual.family = platform::net::AddressFamily::IPv6;
        dual.port = socket_.local().port;
        ipv6_.open(dual);
    }
    return true;
}

void NetworkChannel::close() {
    socket_.close();
    ipv6_.close();
    relay_.reset();
    path_.reset();
    next_ = count_ = 0;
}

bool NetworkChannel::next_relay(uint64_t now) {
    while (next_ < count_) {
        const auto& endpoint = candidates_[next_++];
        Endpoint local;
        local.family = endpoint.family;
        platform::net::Socket socket;
        if (!socket.open(local)) continue;
        if (relay_) {
            if (!relay_->retry_at(endpoint, now)) return false;
            socket_ = std::move(socket);
            return true;
        }
        relay_.emplace(endpoint);
        socket_ = std::move(socket);
        return host_ ? relay_->create(now) : relay_->join(join_code_, now);
    }
    return false;
}

bool NetworkChannel::ready() const {
    return !relay_ || relay_->phase() == net::RelayPhase::Ready;
}

bool NetworkChannel::failed() const {
    if (!relay_) return false;
    const auto phase = relay_->phase();
    return phase == net::RelayPhase::Failed || phase == net::RelayPhase::Refused ||
        (phase == net::RelayPhase::TimedOut && !relay_->code());
}

void NetworkChannel::set_match(const net::SessionKey& key) {
    if (relay_ && !relay_only_) path_.emplace(key);
}

void NetworkChannel::pump(uint64_t now) {
    now_ = now;
    if (!relay_) return;
    if ((relay_->phase() == net::RelayPhase::Ready && now - relay_->last_state() >= RELAY_STALE_MS) ||
        (relay_->phase() == net::RelayPhase::TimedOut && relay_->code())) relay_->rebind(now);
    if (auto request = relay_->poll(now)) {
        socket_.send_to(relay_->relay(), request->bytes, request->size);
    }
    if (relay_->phase() == net::RelayPhase::TimedOut && !relay_->code() && next_ < count_) next_relay(now);
    if (!path_) return;
    const auto& peer = relay_->peer();
    path_->set_candidate(peer.family == socket_.local().family ? peer : Endpoint{});
    if (auto probe = path_->poll(now)) {
        socket_.send_to(path_->candidate(), probe->bytes, probe->size);
    }
}

int NetworkChannel::read(std::span<uint8_t> buffer, Endpoint& from) {
    if (!ipv6_.valid()) return socket_.recv_from(buffer.data(), buffer.size(), &from);
    alternate_ = !alternate_;
    auto& first = alternate_ ? socket_ : ipv6_;
    auto& second = alternate_ ? ipv6_ : socket_;
    const int n = first.recv_from(buffer.data(), buffer.size(), &from);
    return n == 0 ? second.recv_from(buffer.data(), buffer.size(), &from) : n;
}

std::span<const uint8_t> NetworkChannel::logical(Endpoint& from, std::span<const uint8_t> bytes) {
    if (!relay_) return bytes;
    if (from == relay_->relay()) {
        auto packet = relay_->forward(from, bytes);
        if (!packet.empty()) return packet;
        relay_->receive(from, bytes, now_);
        return {};
    }
    if (path_) {
        if (auto reply = path_->receive(from, bytes, now_)) {
            socket_.send_to(from, reply->bytes, reply->size);
        }
        if (path_->accepts(from, bytes)) {
            from = relay_->relay();
            return bytes;
        }
    }
    return {};
}

void NetworkChannel::send(const Endpoint& to, const net::Datagram& packet, bool game) {
    if (!relay_) {
        auto& socket = ipv6_.valid() && to.family == platform::net::AddressFamily::IPv6 ? ipv6_ : socket_;
        socket.send_to(to, packet.bytes, packet.size);
        return;
    }
    if (game && direct() && socket_.send_to(path_->candidate(), packet.bytes, packet.size)) return;
    net::Datagram routed;
    if (relay_->route({packet.bytes, packet.size}, routed)) {
        socket_.send_to(relay_->relay(), routed.bytes, routed.size);
    }
}

} // namespace rumble
