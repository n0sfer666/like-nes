#include "net_peer_path.hpp"
#include "net_wire.hpp"

namespace net {
namespace {

constexpr uint32_t PATH_MAGIC = 0x48544150u;
constexpr size_t PATH_BYTES = 8 + 2 * Token{}.size() + 8;

Role remote(Role local) { return local == Role::Host ? Role::Guest : Role::Host; }

const Token& key_for(const SessionKey& key, Role role) {
    return role == Role::Host ? key.host_key : key.guest_key;
}

} // namespace

void PeerPath::set_candidate(const platform::net::Endpoint& peer) {
    const auto endpoint = platform::net::normalized_endpoint(peer);
    if (candidate_ == endpoint) return;
    candidate_ = endpoint;
    heard_.reset();
    verified_.reset();
    pending_ = false;
    next_ = 0;
}

bool PeerPath::direct(uint64_t now) const {
    return candidate_.port && heard_ && verified_ &&
        now - *heard_ < PATH_LOST_MS && now - *verified_ < PATH_LOST_MS;
}

std::optional<Datagram> PeerPath::packet(Kind kind, uint64_t serial) const {
    const auto& token = key_for(key_, key_.local_role);
    if (!serial || key_.session == Token{} || token == Token{}) return {};
    Datagram out;
    Writer w(out.bytes, sizeof(out.bytes));
    w.u32(PATH_MAGIC);
    w.u16(MATCH_VERSION);
    w.u8(static_cast<uint8_t>(kind));
    w.u8(static_cast<uint8_t>(key_.local_role));
    w.bytes(key_.session.data(), key_.session.size());
    w.bytes(token.data(), token.size());
    w.u64(serial);
    if (!w.ok()) return {};
    out.size = w.size();
    return out;
}

bool PeerPath::open(std::span<const uint8_t> bytes, Kind& kind, uint64_t& serial) const {
    if (bytes.size() != PATH_BYTES) return false;
    Reader r(bytes.data(), bytes.size());
    const Role peer = remote(key_.local_role);
    uint32_t magic = 0;
    uint16_t version = 0;
    uint8_t type = 0, role = 0;
    Token session{}, token{};
    if (!r.u32(&magic) || magic != PATH_MAGIC || !r.u16(&version) || version != MATCH_VERSION ||
        !r.u8(&type) || !r.u8(&role) || role != static_cast<uint8_t>(peer) ||
        !r.bytes(session.data(), session.size()) || session == Token{} || session != key_.session ||
        !r.bytes(token.data(), token.size()) || token == Token{} || token != key_for(key_, peer) ||
        !r.u64(&serial) || !serial) return false;
    kind = static_cast<Kind>(type);
    return kind == Kind::Probe || kind == Kind::Echo;
}

std::optional<Datagram> PeerPath::poll(uint64_t now) {
    if (!candidate_.port || now < next_) return {};
    if (!pending_ || now - sent_at_ >= PATH_LOST_MS) {
        ++serial_;
        sent_at_ = now;
        pending_ = true;
    }
    next_ = now + PATH_PROBE_MS;
    return packet(Kind::Probe, serial_);
}

std::optional<Datagram> PeerPath::receive(const platform::net::Endpoint& from,
                                          std::span<const uint8_t> bytes, uint64_t now) {
    if (!candidate_.port || platform::net::normalized_endpoint(from) != candidate_) return {};
    Kind kind{};
    uint64_t serial = 0;
    if (!open(bytes, kind, serial)) return {};
    if (kind == Kind::Probe) {
        if (serial < peer_serial_) return {};
        if (serial > peer_serial_) {
            peer_serial_ = serial;
            heard_ = now;
        }
        return packet(Kind::Echo, serial);
    }
    if (pending_ && serial == serial_ && now - sent_at_ < PATH_LOST_MS) {
        verified_ = now;
        pending_ = false;
    }
    return {};
}

bool PeerPath::accepts(const platform::net::Endpoint& from, std::span<const uint8_t> bytes) const {
    if (!candidate_.port) return false;
    auto direct_key = key_;
    direct_key.peer = candidate_;
    return !unwrap_session(direct_key, from, bytes).empty();
}

} // namespace net
