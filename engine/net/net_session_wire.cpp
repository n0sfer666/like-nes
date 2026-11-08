#include "net_session_wire.hpp"

#include "hash.hpp"
#include "net_wire.hpp"

namespace net {
namespace {

constexpr uint32_t LOBBY_MAGIC = 0x59424f4cu; // LOBY
constexpr uint32_t SESSION_MAGIC = 0x59414c50u; // PLAY

template<class Wire>
bool fields(Wire& wire, auto& f) {
    return wire.bytes(f.attempt.data(), f.attempt.size()) &&
           wire.bytes(f.proof.data(), f.proof.size()) &&
           wire.bytes(f.session.data(), f.session.size()) &&
           wire.bytes(f.host_key.data(), f.host_key.size()) &&
           wire.bytes(f.guest_key.data(), f.guest_key.size()) &&
           wire.bytes(f.data.data(), f.data.size());
}

bool valid_lobby(const LobbyFrame& f) {
    if (f.attempt == Token{}) return false;
    LobbyFrame canonical;
    canonical.version = f.version;
    canonical.kind = f.kind;
    canonical.attempt = f.attempt;
    if (f.version != MATCH_VERSION && f.version != 0) return f.kind == LobbyKind::Hello;
    if (f.version == 0 && (f.kind != LobbyKind::Reject || f.refusal != Refusal::Version)) return false;
    switch (f.kind) {
        case LobbyKind::Hello:
            canonical.data = f.data;
            break;
        case LobbyKind::Challenge:
        case LobbyKind::Echo:
            if (f.proof == Token{} || f.round >= RTT_ROUNDS || f.probe == 0) return false;
            canonical.proof = f.proof;
            canonical.round = f.round;
            canonical.probe = f.probe;
            break;
        case LobbyKind::Offer:
            if (f.proof == Token{} || f.session == Token{} ||
                f.host_key == Token{} || f.guest_key == Token{} || f.host_key == f.guest_key ||
                f.delay < 1 || f.delay > 3 || f.match_hash != match_hash(f.data, f.delay)) return false;
            canonical.proof = f.proof;
            canonical.session = f.session;
            canonical.host_key = f.host_key;
            canonical.guest_key = f.guest_key;
            canonical.data = f.data;
            canonical.delay = f.delay;
            canonical.match_hash = f.match_hash;
            break;
        case LobbyKind::Ready:
        case LobbyKind::Start:
        case LobbyKind::Started:
            if (f.session == Token{} || f.proof == Token{}) return false;
            canonical.session = f.session;
            canonical.proof = f.proof;
            canonical.match_hash = f.match_hash;
            break;
        case LobbyKind::Reject:
            if (f.refusal < Refusal::Version || f.refusal > Refusal::Settings) return false;
            if (f.refusal == Refusal::Version) {
                if (f.version != 0 || f.proof != Token{} || f.session != Token{}) return false;
            } else if (f.proof == Token{}) {
                return false;
            }
            canonical.refusal = f.refusal;
            canonical.proof = f.proof;
            canonical.session = f.session;
            break;
        default:
            return false;
    }
    return canonical == f;
}

const Token& key_for(const SessionKey& key, Role sender) {
    return sender == Role::Host ? key.host_key : key.guest_key;
}

} // namespace

bool random_token(Token& token) {
    return platform::net::secure_random(token) && token != Token{};
}

bool encode_lobby(const LobbyFrame& f, Datagram& out) {
    out.size = 0;
    if (!valid_lobby(f)) return false;
    Writer w(out.bytes, sizeof(out.bytes));
    w.u32(LOBBY_MAGIC);
    w.u16(f.version);
    w.u8(static_cast<uint8_t>(f.kind));
    w.u8(f.round);
    fields(w, f);
    w.u64(f.match_hash);
    w.u8(f.probe);
    w.u8(f.delay);
    w.u8(static_cast<uint8_t>(f.refusal));
    out.size = w.size();
    return w.ok() && out.size == LOBBY_BYTES;
}

bool decode_lobby(std::span<const uint8_t> bytes, LobbyFrame& out) {
    if (bytes.size() != LOBBY_BYTES) return false;
    Reader r(bytes.data(), bytes.size());
    uint32_t magic = 0;
    uint8_t kind = 0, refusal = 0;
    LobbyFrame f;
    if (!r.u32(&magic) || magic != LOBBY_MAGIC ||
        !r.u16(&f.version) || !r.u8(&kind) || !r.u8(&f.round)) return false;
    f.kind = static_cast<LobbyKind>(kind);
    // The stable prefix ends with the attempt nonce. Version refusals do not
    // interpret the rest of a packet from an incompatible revision.
    if (f.version != MATCH_VERSION && f.version != 0) {
        if (!r.bytes(f.attempt.data(), f.attempt.size()) || !valid_lobby(f)) return false;
    } else {
        if (!fields(r, f) || !r.u64(&f.match_hash) || !r.u8(&f.probe) ||
            !r.u8(&f.delay) || !r.u8(&refusal)) return false;
        f.refusal = static_cast<Refusal>(refusal);
        if (!valid_lobby(f)) return false;
    }
    out = f;
    return true;
}

uint64_t match_hash(const MatchData& data, uint8_t delay) {
    std::array<uint8_t, MatchData{}.size() + 3> canonical{};
    Writer w(canonical.data(), canonical.size());
    w.u16(MATCH_VERSION);
    w.bytes(data.data(), data.size());
    w.u8(delay);
    return asset::fnv1a(canonical.data(), canonical.size());
}

bool wrap_session(const SessionKey& key, std::span<const uint8_t> link, Datagram& out) {
    out.size = 0;
    if (key.local_role != Role::Host && key.local_role != Role::Guest) return false;
    const Token& token = key_for(key, key.local_role);
    if (key.session == Token{} || token == Token{} || link.size() < HEADER_BYTES ||
        link.size() > platform::net::MAX_DATAGRAM - SESSION_BYTES) return false;
    Writer w(out.bytes, sizeof(out.bytes));
    w.u32(SESSION_MAGIC);
    w.u16(MATCH_VERSION);
    w.u8(static_cast<uint8_t>(key.local_role));
    w.u16(static_cast<uint16_t>(link.size()));
    w.bytes(key.session.data(), key.session.size());
    w.bytes(token.data(), token.size());
    w.bytes(link.data(), link.size());
    if (!w.ok()) return false;
    out.size = w.size();
    return true;
}

std::span<const uint8_t> unwrap_session(const SessionKey& key, const platform::net::Endpoint& from,
                                        std::span<const uint8_t> bytes) {
    if (key.local_role != Role::Host && key.local_role != Role::Guest) return {};
    if (platform::net::normalized_endpoint(from) != key.peer || key.session == Token{} ||
        bytes.size() < SESSION_BYTES + HEADER_BYTES || bytes.size() > platform::net::MAX_DATAGRAM) return {};
    const Role sender = key.local_role == Role::Host ? Role::Guest : Role::Host;
    Reader r(bytes.data(), bytes.size());
    uint32_t magic = 0;
    uint16_t version = 0;
    uint8_t role = 0;
    uint16_t length = 0;
    Token session{}, token{};
    if (!r.u32(&magic) || magic != SESSION_MAGIC ||
        !r.u16(&version) || version != MATCH_VERSION ||
        !r.u8(&role) || role != static_cast<uint8_t>(sender) ||
        !r.u16(&length) || length != bytes.size() - SESSION_BYTES ||
        !r.bytes(session.data(), session.size()) || session != key.session ||
        !r.bytes(token.data(), token.size()) || token == Token{} || token != key_for(key, sender)) return {};
    return bytes.subspan(SESSION_BYTES);
}

} // namespace net
