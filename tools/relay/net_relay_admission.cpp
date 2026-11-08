#include "net_relay_server.hpp"

namespace net {
namespace {

RoomCode code_from(const Token& token) {
    RoomCode result = 0;
    for (size_t i = 0; i < 5; ++i) result |= RoomCode{token[i]} << (i * 8);
    return result;
}

size_t index_of(Role role) { return role == Role::Host ? 0 : 1; }

} // namespace

std::optional<RelaySend> RelayServer::intent(const Endpoint& from, const RelayControl& f, uint64_t now) {
    uint64_t binding = 0;
    if (f.action == RelayAction::Rebind) {
        const Room* room = find(f.room);
        if (!room || room->members[index_of(f.role)].key != f.key) return {};
        binding = room->members[index_of(f.role)].binding;
    }
    Candidate* free = nullptr;
    Candidate* selected = nullptr;
    size_t source_count = 0;
    for (auto& c : candidates_) {
        if (!c.deadline) {
            if (!free) free = &c;
            continue;
        }
        auto ip = c.endpoint;
        ip.port = from.port;
        if (ip == from) ++source_count;
        if (c.endpoint == from && c.intent == f) selected = &c;
    }
    const bool stale = selected && selected->binding != binding && !selected->response;
    if (!selected || stale) {
        Token challenge{};
        if ((!selected && (!free || source_count >= 8)) || !random(challenge)) return {};
        if (selected) free = selected;
        *free = Candidate{from, f, challenge, binding, now + RELAY_CHALLENGE_MS, {}};
        selected = free;
    }
    RelayControl response;
    response.kind = RelayKind::Challenge;
    response.action = f.action;
    response.role = f.role;
    response.request = f.request;
    response.challenge = selected->challenge;
    return send(from, response);
}

std::optional<RelaySend> RelayServer::proof(const Endpoint& from, const RelayControl& f, uint64_t now) {
    auto original = f;
    original.kind = RelayKind::Intent;
    original.challenge = {};
    for (auto& c : candidates_) {
        if (!c.deadline || c.endpoint != from || c.challenge != f.challenge || c.intent != original) continue;
        if (!c.response && f.action == RelayAction::Rebind) {
            const auto* room = find(f.room);
            if (room && room->members[index_of(f.role)].binding != c.binding) return {};
        }
        if (!c.response) c.response = action(c, now);
        if (c.response->kind == RelayKind::Grant) {
            const auto* room = find(c.response->room);
            if (!room || room->members[index_of(f.role)].endpoint != from) return {};
        }
        return send(from, *c.response);
    }
    return {};
}

RelayControl RelayServer::action(const Candidate& request, uint64_t now) {
    const auto& f = request.intent;
    RelayControl response;
    response.kind = RelayKind::Grant;
    response.action = f.action;
    response.role = f.role;
    response.request = f.request;
    response.challenge = request.challenge;
    Room* room = nullptr;
    switch (f.action) {
        case RelayAction::Create: response.refusal = create(request, room, now); break;
        case RelayAction::Join: response.refusal = join(request, room); break;
        case RelayAction::Rebind: response.refusal = rebind(request, room); break;
        default: response.refusal = RelayRefusal::Missing; break;
    }
    if (response.refusal != RelayRefusal::None) {
        response.kind = RelayKind::Refuse;
        return response;
    }
    room->touched = now;
    const size_t at = index_of(f.role);
    response.room = room->id;
    response.code = room->code;
    response.key = room->members[at].key;
    response.peer = room->members[1 - at].endpoint;
    return response;
}

RelayRefusal RelayServer::create(const Candidate& request, Room*& room, uint64_t now) {
    for (auto& r : rooms_) {
        if (r.code && r.members[0].request == request.intent.request) {
            room = &r;
            return r.members[0].endpoint == request.endpoint ? RelayRefusal::None : RelayRefusal::Busy;
        }
    }
    room = find(RoomCode{0});
    if (!room) return RelayRefusal::Full;

    Token id{}, key{}, entropy{};
    if (!random(id) || find(id) || !random(key)) return RelayRefusal::Random;
    RoomCode code = 0;
    for (int i = 0; i < 8; ++i) {
        if (!random(entropy)) return RelayRefusal::Random;
        const auto proposal = code_from(entropy);
        if (proposal && !find(proposal)) { code = proposal; break; }
    }
    if (!code) return RelayRefusal::Random;
    *room = Room{id, code, {Member{request.endpoint, key, request.intent.request}, Member{}}, now};
    ++stats_.created;
    return RelayRefusal::None;
}

RelayRefusal RelayServer::join(const Candidate& request, Room*& room) {
    room = find(request.intent.code);
    if (!room) return RelayRefusal::Missing;
    auto& guest = room->members[1];
    if (guest.key != Token{}) {
        return guest.endpoint == request.endpoint && guest.request == request.intent.request ?
            RelayRefusal::None : RelayRefusal::Busy;
    }
    if (request.endpoint == room->members[0].endpoint) return RelayRefusal::Busy;
    Token key{};
    if (!random(key) || key == room->members[0].key) return RelayRefusal::Random;
    guest = Member{request.endpoint, key, request.intent.request};
    return RelayRefusal::None;
}

RelayRefusal RelayServer::rebind(const Candidate& request, Room*& room) {
    room = find(request.intent.room);
    if (!room) return RelayRefusal::Missing;
    const size_t at = index_of(request.intent.role);
    auto& member = room->members[at];
    if (member.key != request.intent.key || member.binding != request.binding) return RelayRefusal::Missing;
    if (room->members[1 - at].endpoint == request.endpoint) return RelayRefusal::Busy;
    member.endpoint = request.endpoint;
    ++member.binding;
    return RelayRefusal::None;
}

} // namespace net
