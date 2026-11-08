#include "net_lobby.hpp"

#include <algorithm>

namespace net {
namespace {

constexpr uint64_t ADMIT_INTERVAL_MS = 25;

bool same_ip(platform::net::Endpoint a, platform::net::Endpoint b) {
    a.port = b.port;
    return a == b;
}

uint8_t delay_from_rtt(std::array<uint64_t, RTT_ROUNDS> rtt) {
    std::sort(rtt.begin(), rtt.end());
    // Median one-way latency rounded up to a 60 Hz tick, clamped to 1..3.
    const uint64_t median = rtt[RTT_ROUNDS / 2];
    if (median <= 33) return 1;
    if (median <= 66) return 2;
    return 3;
}

} // namespace

void Lobby::receive_host(const platform::net::Endpoint& from, const LobbyFrame& f, uint64_t now) {
    if (phase_ == LobbyPhase::Waiting) {
        if (f.kind == LobbyKind::Hello) hello(from, f, now);
        if (f.kind == LobbyKind::Echo) echo(from, f, now);
        return;
    }
    if (from != peer_) return;
    if (phase_ == LobbyPhase::Offered && f.kind == LobbyKind::Reject) {
        auto expected = control(LobbyKind::Reject, Role::Guest);
        expected.match_hash = 0;
        expected.refusal = f.refusal;
        if (f == expected) reject(f.refusal);
        return;
    }
    if (phase_ == LobbyPhase::Offered && f == control(LobbyKind::Ready, Role::Guest)) {
        remote_ready_ = true;
    }
    if (phase_ == LobbyPhase::Starting && f == control(LobbyKind::Started, Role::Guest)) {
        phase_ = LobbyPhase::Started;
    }
}

void Lobby::hello(const platform::net::Endpoint& from, const LobbyFrame& f, uint64_t now) {
    Candidate* free = nullptr;
    size_t ip_slots = 0;
    for (auto& c : candidates_) {
        if (!c.deadline && !free) free = &c;
        if (!c.deadline || !same_ip(c.peer, from)) continue;
        ++ip_slots;
        if (c.peer == from && c.frame.attempt == f.attempt) {
            if (c.frame.round == 0 && c.data == f.data) c.due = true;
            return;
        }
    }
    if (!free || ip_slots >= 2 || now < admit_after_) return;

    LobbyFrame reply;
    reply.attempt = f.attempt;
    if (f.version != MATCH_VERSION) {
        reply.version = 0;
        reply.kind = LobbyKind::Reject;
        reply.refusal = Refusal::Version;
    } else {
        reply.kind = LobbyKind::Challenge;
        if (!new_token(reply.proof)) {
            reject(Refusal::Random);
            return;
        }
    }
    admit_after_ = now + ADMIT_INTERVAL_MS;
    free->peer = from;
    free->data = f.data;
    free->frame = reply;
    free->deadline = now + LOBBY_TIMEOUT_MS;
    free->due = true;
}

void Lobby::echo(const platform::net::Endpoint& from, const LobbyFrame& f, uint64_t now) {
    for (auto& c : candidates_) {
        if (!c.deadline || c.peer != from || c.frame.kind != LobbyKind::Challenge ||
            c.frame.attempt != f.attempt || c.frame.round != f.round ||
            c.frame.proof != f.proof || f.probe > c.frame.probe) continue;
        c.rtt[f.round] = now - c.sent_at[f.probe - 1];
        if (f.round == RTT_ROUNDS - 1) {
            offer(c, now);
        } else {
            ++c.frame.round;
            if (!new_token(c.frame.proof)) reject(Refusal::Random);
            c.frame.probe = 0;
            c.next = now;
        }
        return;
    }
}

void Lobby::offer(Candidate& c, uint64_t now) {
    LobbyFrame reply;
    reply.attempt = c.frame.attempt;
    reply.proof = c.frame.proof;
    reply.refusal = check_settings(c.data);
    if (reply.refusal != Refusal::None) {
        reply.kind = LobbyKind::Reject;
        c.frame = reply;
        c.next = now;
        return;
    }
    if (!new_token(reply.session) || !new_token(reply.host_key) || !new_token(reply.guest_key) ||
        reply.host_key == reply.guest_key) {
        reject(Refusal::Random);
        return;
    }
    reply.kind = LobbyKind::Offer;
    reply.data = config_.data;
    reply.delay = delay_from_rtt(c.rtt);
    reply.match_hash = match_hash(reply.data, reply.delay);

    proposal_ = reply;
    peer_ = c.peer;
    agreement_ = MatchAgreement{{peer_, reply.session, reply.host_key, reply.guest_key, Role::Host},
                                reply.data, reply.match_hash, reply.delay};
    phase_ = LobbyPhase::Offered;
    next_ = now;
    deadline_ = now + LOBBY_READY_TIMEOUT_MS;
    candidates_ = {};
}

std::optional<LobbySend> Lobby::poll_candidate(uint64_t now) {
    for (size_t i = 0; i < candidates_.size(); ++i) {
        const size_t index = (cursor_ + i) % candidates_.size();
        auto& c = candidates_[index];
        if (!c.deadline || now < c.next) continue;
        const bool verified = c.frame.round > 0 ||
            (c.frame.kind == LobbyKind::Reject && c.frame.proof != Token{});
        if (!verified && !c.due) continue;
        if (c.frame.kind == LobbyKind::Challenge) {
            if (c.frame.probe == c.sent_at.size()) continue;
            c.sent_at[c.frame.probe++] = now;
        }
        cursor_ = (index + 1) % candidates_.size();
        c.due = false;
        c.next = now + LOBBY_RETRY_MS;
        return send(c.peer, c.frame);
    }
    return {};
}

std::optional<LobbySend> Lobby::poll_host(uint64_t now) {
    if (phase_ == LobbyPhase::Waiting) return poll_candidate(now);
    if (phase_ == LobbyPhase::Offered && local_ready_ && remote_ready_) {
        phase_ = LobbyPhase::Starting;
        next_ = now;
        deadline_ = now + LOBBY_TIMEOUT_MS;
    }
    if (now < next_) return {};
    if (phase_ == LobbyPhase::Offered) {
        next_ = now + LOBBY_RETRY_MS;
        return send(peer_, proposal_);
    }
    if (phase_ == LobbyPhase::Starting) {
        next_ = now + LOBBY_RETRY_MS;
        return send(peer_, control(LobbyKind::Start, Role::Host));
    }
    return {};
}

} // namespace net
