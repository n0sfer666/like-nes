#include "net_relay_client.hpp"

namespace net {
namespace {

constexpr uint64_t RETRY_MS = 150;
constexpr uint64_t CONTROL_TIMEOUT_MS = 8'000;
constexpr uint64_t QUERY_MS = 500;

} // namespace

bool RelayClient::begin(RelayControl intent, uint64_t now) {
    if (phase_ == RelayPhase::Verifying) return false;
    if (!random_token(intent.request)) {
        phase_ = RelayPhase::Failed;
        return false;
    }
    pending_ = intent;
    verify_again(now);
    return true;
}

void RelayClient::verify_again(uint64_t now) {
    challenge_ = {};
    proof_next_ = false;
    phase_ = RelayPhase::Verifying;
    refusal_ = RelayRefusal::None;
    next_ = now;
    deadline_ = now + CONTROL_TIMEOUT_MS;
}

bool RelayClient::create(uint64_t now) {
    if (room_ != Token{}) return false;
    RelayControl intent;
    intent.action = RelayAction::Create;
    return begin(intent, now);
}

bool RelayClient::join(RoomCode code, uint64_t now) {
    if (!code || code > MAX_ROOM_CODE || room_ != Token{}) return false;
    RelayControl intent;
    intent.action = RelayAction::Join;
    intent.role = Role::Guest;
    intent.code = code;
    return begin(intent, now);
}

bool RelayClient::rebind(uint64_t now) {
    if (room_ == Token{} || key_ == Token{} || phase_ == RelayPhase::Verifying) return false;
    RelayControl intent;
    intent.action = RelayAction::Rebind;
    intent.room = room_;
    intent.key = key_;
    intent.role = role_;
    return begin(intent, now);
}

bool RelayClient::retry_at(const platform::net::Endpoint& endpoint, uint64_t now) {
    if (phase_ != RelayPhase::TimedOut || room_ != Token{}) return false;
    relay_ = platform::net::normalized_endpoint(endpoint);
    verify_again(now);
    return true;
}

void RelayClient::expire(uint64_t now) {
    if (phase_ == RelayPhase::Verifying && now >= deadline_) phase_ = RelayPhase::TimedOut;
}

std::optional<Datagram> RelayClient::poll(uint64_t now) {
    expire(now);
    if (now < next_) return {};
    RelayControl f;
    if (phase_ == RelayPhase::Verifying) {
        f = pending_;
        if (challenge_ != Token{}) {
            if (proof_next_) {
                f.kind = RelayKind::Proof;
                f.challenge = challenge_;
            }
            proof_next_ = !proof_next_;
        }
        next_ = now + RETRY_MS;
    } else if (phase_ == RelayPhase::Ready) {
        if (query_ == Token{} && !random_token(query_)) {
            phase_ = RelayPhase::Failed;
            return {};
        }
        f.kind = RelayKind::Query;
        f.role = role_;
        f.room = room_;
        f.key = key_;
        f.request = query_;
        next_ = now + QUERY_MS;
    } else {
        return {};
    }
    Datagram out;
    if (!encode_relay_control(f, out)) return {};
    return out;
}

void RelayClient::verify(const RelayControl& f, uint64_t now) {
    if (f.request != pending_.request || f.role != pending_.role || f.action != pending_.action) return;
    if (f.kind == RelayKind::Challenge) {
        if (challenge_ != f.challenge) {
            challenge_ = f.challenge;
            proof_next_ = true;
            next_ = now;
        }
        return;
    }
    if (challenge_ == Token{} || f.challenge != challenge_) return;
    if (f.kind == RelayKind::Refuse) {
        phase_ = RelayPhase::Refused;
        refusal_ = f.refusal;
        return;
    }
    if (f.kind != RelayKind::Grant) return;
    if (pending_.action == RelayAction::Join && f.code != pending_.code) return;
    if (pending_.action == RelayAction::Rebind && (f.room != room_ || f.key != key_)) return;
    room_ = f.room;
    key_ = f.key;
    role_ = f.role;
    code_ = f.code;
    peer_ = f.peer;
    query_ = {};
    phase_ = RelayPhase::Ready;
    next_ = now;
    last_state_ = now;
}

void RelayClient::receive(const platform::net::Endpoint& from, std::span<const uint8_t> bytes, uint64_t now) {
    expire(now);
    if (platform::net::normalized_endpoint(from) != relay_) return;
    RelayControl f;
    if (!decode_relay_control(bytes, f)) return;
    if (phase_ == RelayPhase::Verifying) {
        verify(f, now);
    } else if (phase_ == RelayPhase::Ready && f.kind == RelayKind::State && f.room == room_ &&
               f.key == key_ && f.role == role_ && f.request == query_) {
        peer_ = f.peer;
        query_ = {};
        last_state_ = now;
    }
}

bool RelayClient::route(std::span<const uint8_t> payload, Datagram& out) const {
    return encode_relay_route({RelayKind::Route, role_, room_, key_, payload}, out);
}

std::span<const uint8_t> RelayClient::forward(const platform::net::Endpoint& from,
                                              std::span<const uint8_t> bytes) const {
    if (room_ == Token{} || platform::net::normalized_endpoint(from) != relay_) return {};
    RelayRoute f;
    if (!decode_relay_route(bytes, f) || f.kind != RelayKind::Forward ||
        f.room != room_ || f.key != key_ || f.role != role_) return {};
    return f.payload;
}

} // namespace net
