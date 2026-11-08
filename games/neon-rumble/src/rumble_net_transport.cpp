#include "rumble_net_fight.hpp"

#include <algorithm>

namespace rumble {
namespace {

constexpr size_t PENDING_LIMIT = 4;
constexpr uint64_t SEND_INTERVAL_MS = 16;

} // namespace

void NetworkFight::queue(uint64_t now) {
    if (now < next_send_ || link_.unacked() >= PENDING_LIMIT) return;
    NetworkInputPacket p;
    p.begin = peer_known_;
    p.confirmed = known_;
    p.count = static_cast<uint8_t>(std::min(written_ - p.begin, NETWORK_INPUTS));
    for (uint32_t i = 0; i < p.count; ++i) {
        const auto at = p.begin + i;
        p.inputs[i] = inputs_[local_seat_][at % NETWORK_HISTORY].command;
    }
    const uint32_t high = confirmed();
    p.hash_count = static_cast<uint8_t>(std::min(high + 1, NETWORK_INPUTS));
    p.hash_tick = high + 1 - p.hash_count;
    for (uint32_t i = 0; i < p.hash_count; ++i) {
        const auto hash = sim_.hash_at(p.hash_tick + i);
        if (!hash) {
            phase_ = NetworkPhase::Failed;
            return;
        }
        p.hashes[i] = *hash;
    }
    p.probe = probe_ + 1;
    p.echo = peer_probe_;

    std::array<uint8_t, net::SESSION_PAYLOAD> bytes{};
    const size_t n = encode_network_input(p, bytes);
    if (!n) {
        phase_ = NetworkPhase::Failed;
        return;
    }
    if (!link_.send(bytes.data(), n)) return;
    probe_ = p.probe;
    probes_[probe_ % probes_.size()] = {probe_, now};
    next_send_ = now + SEND_INTERVAL_MS;
}

void NetworkFight::pump(uint64_t now, Send send, void* user) {
    expire(now);
    if (phase_ == NetworkPhase::Desynced) notify(now, send, user);
    if (!active()) return;
    queue(now);
    if (!active()) return;
    link_.pump(static_cast<uint32_t>(now));
    for (size_t i = 0; i < link_.outbox_size(); ++i) {
        const auto& inner = link_.outbox_at(i);
        net::Datagram packet;
        if (net::wrap_session(key_, {inner.bytes, inner.size}, packet)) send(user, key_.peer, packet);
    }
}

bool NetworkFight::integrate(const NetworkInputPacket& p) {
    if (p.probe <= peer_probe_ || p.confirmed > written_) return false;
    const auto head = rollback_.tick();
    auto& history = inputs_[remote_seat_];
    // Preflight the entire range before committing any part of the datagram.
    for (uint32_t i = 0; i < p.count; ++i) {
        const auto at = p.begin + i;
        if (at > head + rollback_.LEAD) return false;
        const auto& entry = history[at % NETWORK_HISTORY];
        if (entry.tick == at && !(entry.command == p.inputs[i])) {
            phase_ = NetworkPhase::Failed;
            return false;
        }
    }
    for (uint32_t i = 0; i < p.count; ++i) {
        const auto at = p.begin + i;
        if (at >= known_ && history[at % NETWORK_HISTORY].tick != at) store(at, remote_seat_, p.inputs[i]);
    }
    if (!active()) return false;
    peer_probe_ = p.probe;
    peer_known_ = std::max(peer_known_, p.confirmed);
    refresh_known();
    rollback_.settle(sim_);
    return true;
}

bool NetworkFight::proof(const NetworkInputPacket& p, uint64_t now) {
    if (p.echo <= echo_) return false;
    const auto& sent = probes_[p.echo % probes_.size()];
    if (sent.serial != p.echo || now - sent.at >= NETWORK_START_MS ||
        (phase_ == NetworkPhase::Paused && sent.at < entered_at_)) return false;
    echo_ = p.echo;
    rtt_ms_ = static_cast<uint32_t>(now - sent.at);
    return true;
}

bool NetworkFight::check_hashes(const NetworkInputPacket& p, uint64_t now) {
    bool matched = false;
    for (uint32_t i = 0; i < p.hash_count; ++i) {
        const auto at = p.hash_tick + i;
        if (at > confirmed()) break;
        const auto hash = sim_.hash_at(at);
        if (!hash) continue;
        if (*hash != p.hashes[i]) {
            desync(at, now);
            return false;
        }
        matched = true;
    }
    return matched;
}

void NetworkFight::receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now) {
    expire(now);
    if (!active() && phase_ != NetworkPhase::Desynced) return;
    const auto opened = net::unwrap_session(key_, from, bytes);
    if (opened.empty()) return;
    if (accept_notice(opened, now) || !active()) return;
    std::array<uint8_t, net::SESSION_PAYLOAD> payload{};
    size_t size = 0;
    if (link_.receive(opened.data(), opened.size(), payload.data(), payload.size(), &size) !=
        net::Received::Delivered) return;
    NetworkInputPacket p;
    if (!decode_network_input({payload.data(), size}, p) || !integrate(p)) return;
    if (!check_hashes(p, now) || !proof(p, now)) return;
    alive_at_ = now;
    if (phase_ == NetworkPhase::Paused) ++reconnects_;
    phase_ = NetworkPhase::Playing;
}

} // namespace rumble
