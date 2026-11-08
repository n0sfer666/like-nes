#include "relay_test_fixture.hpp"

namespace {
using namespace relay_test;

constexpr Endpoint HOST{0x7f000001u, 4700}, GUEST{0x7f000001u, 4701}, MOVED{0x7f000002u, 4702};

Datagram payload(const RelayControl& owner, Role role) {
    const uint8_t data[] = {4, 8, 15, 16, 23, 42};
    Datagram out;
    require(encode_relay_route({RelayKind::Route, role, owner.room, owner.key, data}, out), "route encode");
    return out;
}

void forwards(RelayServer& server, Endpoint from, Endpoint to, const RelayControl& owner,
              const RelayControl& target, uint64_t now) {
    const auto datagram = payload(owner, owner.role);
    auto sent = receive(server, from, datagram, now);
    RelayRoute route;
    require(sent && sent->to == to &&
            decode_relay_route({sent->packet.bytes, sent->packet.size}, route) &&
            route.kind == RelayKind::Forward && route.role == target.role && route.key == target.key &&
            route.room == owner.room && route.payload.size() == 6 && route.payload.back() == 42,
            "payload reaches only the verified destination");
}

void lifecycle() {
    RelayServer server(serial);
    const auto create = start(RelayAction::Create, Role::Host);
    const auto create_proof = proof(server, HOST, create, 0);
    require(!call(server, GUEST, create_proof, 1), "proof from other address rejected");
    const auto host = grant(server, HOST, create_proof, 2);
    require(host.peer.port == 0 && host.code != 0 && host.key != host.room, "distinct room and host key");
    require(grant(server, HOST, create_proof, 3) == host && server.stats().created == 1,
            "repeated Create yields same room and role token");
    const auto other_address = call(server, MOVED, proof(server, MOVED, create, 3), 3);
    require(other_address && other_address->refusal == RelayRefusal::Busy && server.stats().created == 1,
            "same create nonce at another address neither allocates nor exposes keys");

    auto join = start(RelayAction::Join, Role::Guest);
    join.code = host.code;
    const auto join_proof = proof(server, GUEST, join, 4);
    const auto guest = grant(server, GUEST, join_proof, 5);
    require(guest.peer == HOST && guest.room == host.room && guest.key != host.key,
            "guest has its own key and sees host endpoint");
    require(grant(server, GUEST, join_proof, 6) == guest && server.stats().created == 1,
            "repeated Join reuses guest");
    serial(join.request);
    const auto denied = call(server, MOVED, proof(server, MOVED, join, 6), 6);
    require(denied && denied->refusal == RelayRefusal::Busy && denied->room == Token{}, "room cannot be stolen");

    forwards(server, HOST, GUEST, host, guest, 7);
    forwards(server, GUEST, HOST, guest, host, 8);
    require(!receive(server, MOVED, payload(host, Role::Host), 9), "token alone cannot change address");
    require(!receive(server, HOST, payload(host, Role::Guest), 9), "token alone cannot change role");

    auto rebind = start(RelayAction::Rebind, Role::Host);
    rebind.room = host.room;
    rebind.key = guest.key;
    require(!call(server, MOVED, rebind, 10), "wrong token cannot request host rebind");
    rebind.key = host.key;
    const auto rebind_proof = proof(server, MOVED, rebind, 10);
    require(!call(server, HOST, rebind_proof, 11), "rebind proof is tied to new address");
    const auto moved = grant(server, MOVED, rebind_proof, 12);
    require(moved.room == host.room && moved.key == host.key && moved.peer == GUEST,
            "rebind preserves room and key");
    require(!call(server, HOST, create_proof, 12), "old address cannot recover cached grant");
    require(!receive(server, HOST, payload(host, Role::Host), 13), "old route closed");
    forwards(server, GUEST, MOVED, guest, moved, 14);
    forwards(server, MOVED, GUEST, moved, guest, 15);

    auto query = start(RelayAction::None, Role::Guest);
    query.kind = RelayKind::Query;
    query.room = guest.room;
    query.key = guest.key;
    auto state = call(server, GUEST, query, 16);
    require(state && state->kind == RelayKind::State && state->peer == MOVED, "candidate follows rebind");

    const auto retry = proof(server, HOST, create, RELAY_CHALLENGE_MS + 20);
    const auto duplicate = call(server, HOST, retry, RELAY_CHALLENGE_MS + 21);
    require(duplicate && duplicate->refusal == RelayRefusal::Busy && server.stats().created == 1,
            "retry from original address does not allocate second room");

    const auto expired = 16 + RELAY_ROOM_MS;
    require(!receive(server, MOVED, payload(moved, Role::Host), expired) && server.stats().expired == 1,
            "idle room expires exactly at TTL");
    const auto replacement = grant(server, HOST, proof(server, HOST, start(RelayAction::Create, Role::Host), expired),
                                   expired);
    require(replacement.room != host.room && replacement.code != host.code && server.stats().created == 2,
            "expired slot can be used again");
    require(!receive(server, MOVED, payload(moved, Role::Host), expired + 1), "old room route stays closed");
}

void stale_rebind() {
    RelayServer server(serial);
    constexpr Endpoint newest{0x7f000003u, 4703};
    const auto create = start(RelayAction::Create, Role::Host);
    const auto host = grant(server, HOST, proof(server, HOST, create, 0), 0);
    auto join = start(RelayAction::Join, Role::Guest);
    join.code = host.code;
    const auto guest = grant(server, GUEST, proof(server, GUEST, join, 1), 1);
    auto rebind = start(RelayAction::Rebind, Role::Host);
    rebind.room = host.room;
    rebind.key = host.key;
    const auto old = proof(server, MOVED, rebind, 2);
    serial(rebind.request);
    const auto current = proof(server, newest, rebind, 3);
    grant(server, newest, current, 4);
    const auto stale = call(server, MOVED, old, 5);
    require(!stale, "older binding proof cannot displace a newer binding");
    forwards(server, GUEST, newest, guest, host, 6);
    auto retry = old;
    retry.kind = RelayKind::Intent;
    retry.challenge = {};
    const auto renewed = proof(server, MOVED, retry, 7);
    require(renewed.challenge != old.challenge, "same request can verify against the newer binding");
    grant(server, MOVED, renewed, 8);
    require(!call(server, newest, current, 9), "old cached proof cannot restore an obsolete route");
    forwards(server, GUEST, MOVED, guest, host, 10);
}


} // namespace

void relay_rate_and_capacity();

int main() {
    lifecycle();
    stale_rebind();
    relay_rate_and_capacity();
    std::puts("relay-server: PASS");
}
