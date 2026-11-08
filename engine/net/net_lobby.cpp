#include "net_lobby.hpp"

namespace net {

void Lobby::listen() {
    *this = Lobby(config_, random_);
}

bool Lobby::join(const platform::net::Endpoint& host, uint64_t now) {
    listen();
    role_ = Role::Guest;
    peer_ = platform::net::normalized_endpoint(host);
    if (!new_token(pending_.attempt)) {
        reject(Refusal::Random);
        return false;
    }
    phase_ = LobbyPhase::Probing;
    pending_.data = config_.data;
    deadline_ = now + LOBBY_TIMEOUT_MS;
    return true;
}

void Lobby::reject(Refusal reason) {
    phase_ = LobbyPhase::Rejected;
    refusal_ = reason;
    agreement_.reset();
}

Refusal Lobby::check_settings(const MatchData& remote) const {
    if (!config_.check) return Refusal::None;
    const auto reason = config_.check(config_.data, remote);
    if (reason == Refusal::None || (reason >= Refusal::Data && reason <= Refusal::Settings)) return reason;
    return Refusal::Settings;
}

void Lobby::expire(uint64_t now) {
    if (phase_ == LobbyPhase::Started || phase_ == LobbyPhase::Rejected) return;
    if (deadline_ && now >= deadline_) reject(Refusal::Timeout);
    for (auto& candidate : candidates_) {
        if (candidate.deadline && now >= candidate.deadline) candidate = {};
    }
}

void Lobby::receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now) {
    expire(now);
    const auto source = platform::net::normalized_endpoint(from);
    if (role_ == Role::Guest && source != peer_) return;
    LobbyFrame f;
    if (!decode_lobby(bytes, f)) return;
    if (role_ == Role::Host) receive_host(source, f, now);
    else receive_guest(f, now);
}

std::optional<LobbySend> Lobby::poll(uint64_t now) {
    expire(now);
    if (phase_ == LobbyPhase::Rejected && role_ == Role::Host) return {};
    return role_ == Role::Host ? poll_host(now) : poll_guest(now);
}

std::optional<LobbySend> Lobby::send(const platform::net::Endpoint& to, const LobbyFrame& f) {
    LobbySend result;
    result.to = to;
    if (!encode_lobby(f, result.datagram)) return {};
    return result;
}

LobbyFrame Lobby::control(LobbyKind kind, Role sender) const {
    LobbyFrame f;
    f.kind = kind;
    f.attempt = proposal_.attempt;
    f.session = proposal_.session;
    f.proof = sender == Role::Host ? proposal_.host_key : proposal_.guest_key;
    f.match_hash = proposal_.match_hash;
    return f;
}

void Lobby::receive_guest(const LobbyFrame& f, uint64_t now) {
    if (phase_ == LobbyPhase::Rejected || f.attempt != pending_.attempt) return;
    if (phase_ == LobbyPhase::Probing) {
        if (f.kind == LobbyKind::Reject) {
            const bool version = f.refusal == Refusal::Version && pending_.kind == LobbyKind::Hello;
            const bool challenged = pending_.kind == LobbyKind::Echo &&
                pending_.round == RTT_ROUNDS - 1 && f.proof == pending_.proof;
            if (version || challenged) reject(f.refusal);
        }
        if (f.kind == LobbyKind::Challenge) {
            if (pending_.kind == LobbyKind::Hello && f.round != 0) return;
            if (pending_.kind == LobbyKind::Echo && f.round != pending_.round + 1 &&
                !(f.round == pending_.round && f.proof == pending_.proof && f.probe > pending_.probe)) return;
            pending_ = f;
            pending_.kind = LobbyKind::Echo;
            reply_due_ = true;
        }
        if (f.kind == LobbyKind::Offer && pending_.kind == LobbyKind::Echo &&
            pending_.round == RTT_ROUNDS - 1 && f.proof == pending_.proof) {
            const Refusal reason = check_settings(f.data);
            if (reason != Refusal::None) {
                proposal_ = f;
                next_ = now;
                reject(reason);
                return;
            }
            proposal_ = f;
            agreement_ = MatchAgreement{{peer_, f.session, f.host_key, f.guest_key, Role::Guest},
                                        f.data, f.match_hash, f.delay};
            phase_ = LobbyPhase::Offered;
            reply_due_ = false;
            next_ = now;
            deadline_ = now + LOBBY_READY_TIMEOUT_MS;
        }
        return;
    }
    if ((phase_ != LobbyPhase::Offered && phase_ != LobbyPhase::Started) ||
        !local_ready_ || f != control(LobbyKind::Start, Role::Host)) return;
    phase_ = LobbyPhase::Started;
    reply_due_ = true;
}

std::optional<LobbySend> Lobby::poll_guest(uint64_t now) {
    if (phase_ == LobbyPhase::Probing) {
        if (reply_due_) {
            reply_due_ = false;
            return send(peer_, pending_);
        }
        if (now < next_ || pending_.round != 0) return {};
        next_ = now + LOBBY_RETRY_MS;
        LobbyFrame hello;
        hello.attempt = pending_.attempt;
        hello.data = config_.data;
        return send(peer_, hello);
    }
    if (now < next_) return {};
    if (phase_ == LobbyPhase::Offered && local_ready_) {
        next_ = now + LOBBY_RETRY_MS;
        return send(peer_, control(LobbyKind::Ready, Role::Guest));
    }
    if (phase_ == LobbyPhase::Started && reply_due_) {
        next_ = now + LOBBY_RETRY_MS;
        reply_due_ = false;
        return send(peer_, control(LobbyKind::Started, Role::Guest));
    }
    if (phase_ == LobbyPhase::Rejected && proposal_.session != Token{} && now < deadline_) {
        auto refusal = control(LobbyKind::Reject, Role::Guest);
        refusal.refusal = refusal_;
        refusal.match_hash = 0;
        next_ = now + LOBBY_RETRY_MS;
        return send(peer_, refusal);
    }
    return {};
}

} // namespace net
