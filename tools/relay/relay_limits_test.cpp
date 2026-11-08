#include "relay_test_fixture.hpp"
#include <memory>

namespace {
using namespace relay_test;

void packet_and_byte_limit() {
    RelayServer server(serial);
    uint8_t small = 0;
    Endpoint ip{0x7f000001u, 4700};
    for (uint32_t i = 0; i < RELAY_PACKETS_PER_IP; ++i) {
        ++ip.port;
        require(!server.receive(ip, {&small, 1}, 0), "malformed packet ignored");
    }
    Endpoint mapped;
    require(platform::net::parse_endpoint("[::ffff:127.0.0.1]:1", &mapped), "mapped source");
    server.receive(mapped, {&small, 1}, 0);
    require(server.stats().limited == 1, "per-IP packet cap includes mapped address and every port");
    server.receive(ip, {&small, 1}, 1'000);
    require(server.stats().limited == 1, "new packet window replenishes");

    const uint8_t big[platform::net::MAX_DATAGRAM]{};
    const size_t limit = RELAY_BYTES_PER_IP / sizeof(big);
    for (size_t i = 0; i <= limit; ++i) server.receive(ip, big, 2'000);
    require(server.stats().limited == 2, "byte limit applies before parsing");
    require(server.stats().bytes == RELAY_PACKETS_PER_IP + 2 + (limit + 1) * sizeof(big), "input byte counter");
}

void bounded_tables() {
    auto server = std::make_unique<RelayServer>(serial);
    for (size_t i = 0; i < RELAY_CHALLENGES; ++i) {
        const Endpoint from{0x7f000001u + static_cast<uint32_t>(i), 4700};
        const auto reply = call(*server, from, start(RelayAction::Create, Role::Host), 0);
        require(reply && reply->kind == RelayKind::Challenge, "candidate fits");
    }
    const Endpoint newcomer{0x7f001000u, 4700};
    const auto intent = start(RelayAction::Create, Role::Host);
    require(!call(*server, newcomer, intent, 0), "candidate capacity is hard bound");
    const auto expired = proof(*server, newcomer, intent, RELAY_CHALLENGE_MS);
    require(!call(*server, newcomer, expired, 2 * RELAY_CHALLENGE_MS), "expired challenge cannot create room");

    server = std::make_unique<RelayServer>(serial);
    for (size_t i = 0; i < RELAY_ROOMS; ++i) {
        const Endpoint from{0x7f000001u + static_cast<uint32_t>(i), 4700};
        grant(*server, from, proof(*server, from, start(RelayAction::Create, Role::Host), 0), 0);
    }
    const auto full = call(*server, newcomer, proof(*server, newcomer, intent, 0), 0);
    require(full && full->refusal == RelayRefusal::Full && server->stats().created == RELAY_ROOMS,
            "room memory and error output are bounded");
    server->expire(RELAY_ROOM_MS);
    require(server->stats().expired == RELAY_ROOMS, "all rooms age out");
    grant(*server, newcomer, proof(*server, newcomer, intent, RELAY_ROOM_MS), RELAY_ROOM_MS);
}

void global_budget() {
    RelayServer server(serial);
    const uint8_t byte = 0;
    for (uint32_t i = 0; i <= 4096; ++i) {
        const Endpoint from{0x7f000001u + i % static_cast<uint32_t>(RELAY_RATE_SOURCES), 4700};
        server.receive(from, {&byte, 1}, 0);
    }
    require(server.stats().limited == 1, "global cap applies across IPs");
    const Endpoint extra{0x7f001000u, 4700};
    server.receive(extra, {&byte, 1}, 1'000);
    require(server.stats().limited == 2, "source table is bounded");
    const auto intent = start(RelayAction::Create, Role::Host);
    require(call(server, extra, intent, 10'000).has_value(), "inactive IP records are reusable");
    require(!call(server, Endpoint{0xe0000001u, 4700}, intent, 10'001),
            "multicast source cannot get a reply");

    RelayServer fairness(serial);
    const Endpoint noisy{0x7f000001u, 4700};
    for (uint32_t i = 0; i < 4096; ++i) fairness.receive(noisy, {&byte, 1}, 0);
    require(call(fairness, extra, intent, 0).has_value(),
            "traffic above one IP limit cannot consume the common allowance");
}

} // namespace

void relay_rate_and_capacity() {
    packet_and_byte_limit();
    bounded_tables();
    global_budget();
}
