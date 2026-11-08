#include "net_relay_wire.hpp"
#include "net_relay_endpoint.hpp"

namespace net {
namespace {

constexpr uint32_t MAGIC = 0x31594c52u; // RLY1
constexpr uint8_t VERSION = 1;

bool has_role(Role role) { return role == Role::Host || role == Role::Guest; }

bool has_action(const RelayControl& f) {
    switch (f.action) {
        case RelayAction::Create: return f.role == Role::Host;
        case RelayAction::Join: return f.role == Role::Guest;
        case RelayAction::Rebind: return true;
        default: return false;
    }
}

bool valid(const RelayControl& f) {
    if (!has_role(f.role) || f.request == Token{} || f.code > MAX_ROOM_CODE) return false;
    RelayControl expected;
    expected.kind = f.kind;
    expected.role = f.role;
    expected.request = f.request;
    switch (f.kind) {
        case RelayKind::Intent:
        case RelayKind::Proof:
            if (!has_action(f)) return false;
            expected.action = f.action;
            if (f.kind == RelayKind::Proof) {
                if (f.challenge == Token{}) return false;
                expected.challenge = f.challenge;
            }
            if (f.action == RelayAction::Join) {
                if (!f.code) return false;
                expected.code = f.code;
            }
            if (f.action == RelayAction::Rebind) {
                if (f.room == Token{} || f.key == Token{}) return false;
                expected.room = f.room;
                expected.key = f.key;
            }
            break;
        case RelayKind::Challenge:
        case RelayKind::Refuse:
        case RelayKind::Grant:
            if (!has_action(f) || f.challenge == Token{}) return false;
            expected.action = f.action;
            expected.challenge = f.challenge;
            if (f.kind == RelayKind::Refuse) {
                if (f.refusal < RelayRefusal::Missing || f.refusal > RelayRefusal::Random) return false;
                expected.refusal = f.refusal;
            }
            if (f.kind == RelayKind::Grant) {
                if (!f.code || f.room == Token{} || f.key == Token{}) return false;
                expected.code = f.code;
                expected.room = f.room;
                expected.key = f.key;
                expected.peer = f.peer;
            }
            break;
        case RelayKind::Query:
        case RelayKind::State:
            if (f.room == Token{} || f.key == Token{}) return false;
            expected.room = f.room;
            expected.key = f.key;
            if (f.kind == RelayKind::State) expected.peer = f.peer;
            break;
        default:
            return false;
    }
    return expected == f;
}

template<class Wire>
bool token(Wire& w, auto& field) { return w.bytes(field.data(), field.size()); }

void prefix(Writer& w, RelayKind kind, Role role, uint8_t action, uint8_t status) {
    w.u32(MAGIC);
    w.u8(VERSION);
    w.u8(static_cast<uint8_t>(kind));
    w.u8(static_cast<uint8_t>(role));
    w.u8(static_cast<uint8_t>((action << 4) | status));
}

bool prefix(Reader& r, RelayKind& kind, Role& role, uint8_t& flags) {
    uint32_t magic = 0;
    uint8_t version = 0, type = 0, who = 0;
    if (!r.u32(&magic) || magic != MAGIC || !r.u8(&version) || version != VERSION ||
        !r.u8(&type) || !r.u8(&who) || !r.u8(&flags)) return false;
    kind = static_cast<RelayKind>(type);
    role = static_cast<Role>(who);
    return has_role(role);
}

bool finish(Writer& w, Datagram& out) {
    if (!w.ok()) return false;
    out.size = w.size();
    return true;
}

} // namespace

bool encode_relay_control(const RelayControl& f, Datagram& out) {
    out.size = 0;
    if (!valid(f)) return false;
    Writer w(out.bytes, sizeof(out.bytes));
    prefix(w, f.kind, f.role, static_cast<uint8_t>(f.action), static_cast<uint8_t>(f.refusal));
    token(w, f.request);
    token(w, f.challenge);
    token(w, f.room);
    token(w, f.key);
    w.u64(f.code);
    return relay_detail::endpoint(w, f.peer) && finish(w, out);
}

bool decode_relay_control(std::span<const uint8_t> bytes, RelayControl& out) {
    if (bytes.size() != RELAY_CONTROL_BYTES) return false;
    Reader r(bytes.data(), bytes.size());
    RelayControl f;
    uint8_t flags = 0;
    if (!prefix(r, f.kind, f.role, flags) || !token(r, f.request) || !token(r, f.challenge) ||
        !token(r, f.room) || !token(r, f.key) || !r.u64(&f.code) || !relay_detail::endpoint(r, f.peer)) return false;
    f.action = static_cast<RelayAction>(flags >> 4);
    f.refusal = static_cast<RelayRefusal>(flags & 0x0f);
    if (!valid(f)) return false;
    out = f;
    return true;
}

bool encode_relay_route(const RelayRoute& f, Datagram& out) {
    out.size = 0;
    if ((f.kind != RelayKind::Route && f.kind != RelayKind::Forward) || !has_role(f.role) ||
        f.room == Token{} || f.key == Token{} || f.payload.empty() || f.payload.size() > RELAY_PAYLOAD) return false;
    Writer w(out.bytes, sizeof(out.bytes));
    prefix(w, f.kind, f.role, 0, 0);
    token(w, f.room);
    token(w, f.key);
    w.u16(static_cast<uint16_t>(f.payload.size()));
    w.bytes(f.payload.data(), f.payload.size());
    return finish(w, out);
}

bool decode_relay_route(std::span<const uint8_t> bytes, RelayRoute& out) {
    if (bytes.size() <= RELAY_ROUTE_BYTES || bytes.size() > platform::net::MAX_DATAGRAM) return false;
    Reader r(bytes.data(), bytes.size());
    RelayRoute f;
    uint8_t flags = 0;
    uint16_t length = 0;
    if (!prefix(r, f.kind, f.role, flags) || flags != 0 ||
        (f.kind != RelayKind::Route && f.kind != RelayKind::Forward) ||
        !token(r, f.room) || f.room == Token{} || !token(r, f.key) || f.key == Token{} ||
        !r.u16(&length) || length != r.remaining()) return false;
    f.payload = bytes.subspan(RELAY_ROUTE_BYTES);
    out = f;
    return true;
}

} // namespace net
