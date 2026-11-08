#include "net_relay_server.hpp"
#include "net_relay_endpoint.hpp"

namespace net {
namespace {

constexpr uint64_t WINDOW_MS = 1000;
constexpr uint64_t SOURCE_MS = 10'000;
constexpr uint32_t GLOBAL_PACKETS = 4096;
constexpr uint32_t GLOBAL_BYTES = 2 * 1024 * 1024;

constexpr size_t slot(Role role) { return role == Role::Host ? 0 : 1; }
constexpr Role other(Role role) { return role == Role::Host ? Role::Guest : Role::Host; }

} // namespace

bool RelayServer::Budget::admit(uint64_t now, uint32_t size, uint32_t max_packets, uint32_t max_bytes) {
    if (now - window >= WINDOW_MS) {
        window = now;
        packets = 0;
        bytes = 0;
    }
    if (packets == max_packets || size > max_bytes - bytes) return false;
    ++packets;
    bytes += size;
    return true;
}

bool RelayServer::admit(const Endpoint& from, uint32_t size, uint64_t now) {
    Endpoint ip = from;
    ip.port = 0;
    Source* free = nullptr;
    for (auto& entry : sources_) {
        if (entry.expires <= now) {
            if (!free) free = &entry;
        } else if (entry.ip == ip) {
            entry.expires = now + SOURCE_MS;
            return entry.budget.admit(now, size, RELAY_PACKETS_PER_IP, RELAY_BYTES_PER_IP) &&
                global_.admit(now, size, GLOBAL_PACKETS, GLOBAL_BYTES);
        }
    }
    if (!free) return false;
    *free = Source{ip, now + SOURCE_MS, Budget{now}};
    return free->budget.admit(now, size, RELAY_PACKETS_PER_IP, RELAY_BYTES_PER_IP) &&
        global_.admit(now, size, GLOBAL_PACKETS, GLOBAL_BYTES);
}

void RelayServer::expire(uint64_t now) {
    for (auto& c : candidates_) {
        if (c.deadline && now >= c.deadline) c = {};
    }
    for (auto& room : rooms_) {
        if (room.code && now - room.touched >= RELAY_ROOM_MS) {
            room = {};
            ++stats_.expired;
        }
    }
}

RelayServer::Room* RelayServer::find(const Token& id) {
    for (auto& room : rooms_) {
        if (room.code && room.id == id) return &room;
    }
    return nullptr;
}

RelayServer::Room* RelayServer::find(RoomCode code) {
    for (auto& room : rooms_) {
        if (room.code == code) return &room;
    }
    return nullptr;
}

std::optional<RelaySend> RelayServer::send(const Endpoint& to, const RelayControl& f) {
    RelaySend result;
    result.to = to;
    if (!encode_relay_control(f, result.packet)) return {};
    return result;
}

std::optional<RelaySend> RelayServer::query(const Endpoint& from, const RelayControl& f, uint64_t now) {
    Room* room = find(f.room);
    if (!room) return {};
    auto& member = room->members[slot(f.role)];
    if (member.key != f.key || member.endpoint != from) return {};
    RelayControl response = f;
    response.kind = RelayKind::State;
    response.peer = room->members[slot(other(f.role))].endpoint;
    room->touched = now;
    return send(from, response);
}

std::optional<RelaySend> RelayServer::route(const Endpoint& from, const RelayRoute& f, uint64_t now) {
    Room* room = find(f.room);
    if (!room) return {};
    const auto& sender = room->members[slot(f.role)];
    const auto& target = room->members[slot(other(f.role))];
    if (sender.key != f.key || sender.endpoint != from || target.key == Token{}) return {};
    RelaySend result;
    result.to = target.endpoint;
    if (!encode_relay_route({RelayKind::Forward, other(f.role), f.room, target.key, f.payload}, result.packet)) {
        return {};
    }
    room->touched = now;
    ++stats_.forwarded;
    return result;
}

std::optional<RelaySend> RelayServer::receive(const Endpoint& from, std::span<const uint8_t> bytes, uint64_t now) {
    ++stats_.packets;
    stats_.bytes += bytes.size();
    const auto source = platform::net::normalized_endpoint(from);
    if (!relay_detail::unicast(source) || bytes.empty() || bytes.size() > platform::net::MAX_DATAGRAM) {
        ++stats_.rejected;
        return {};
    }
    if (!admit(source, static_cast<uint32_t>(bytes.size()), now)) {
        ++stats_.limited;
        return {};
    }
    expire(now);
    std::optional<RelaySend> result;
    RelayControl f;
    RelayRoute r;
    if (decode_relay_control(bytes, f)) {
        switch (f.kind) {
            case RelayKind::Intent: result = intent(source, f, now); break;
            case RelayKind::Proof: result = proof(source, f, now); break;
            case RelayKind::Query: result = query(source, f, now); break;
            default: break;
        }
    } else if (decode_relay_route(bytes, r) && r.kind == RelayKind::Route) {
        result = route(source, r, now);
    }
    if (!result || result->packet.size > bytes.size()) {
        ++stats_.rejected;
        return {};
    }
    return result;
}

} // namespace net
