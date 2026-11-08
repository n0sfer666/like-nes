#include "net_peer_path.hpp"
#include "net_relay_client.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
using namespace net;
using platform::net::Endpoint;
constexpr Endpoint HOST{0x7f000001u, 4800}, GUEST{0x7f000001u, 4801}, RELAY{0x7f000001u, 4802};

void require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "net-relay-path: FAIL %s\n", what);
    std::exit(1);
}

std::span<const uint8_t> bytes(const Datagram& packet) { return {packet.bytes, packet.size}; }

void paths() {
    SessionKey host_key{RELAY};
    host_key.session[0] = 1;
    host_key.host_key[0] = 2;
    host_key.guest_key[0] = 3;
    SessionKey guest_key = host_key;
    guest_key.local_role = Role::Guest;
    PeerPath host(host_key), guest(guest_key);
    host.set_candidate(GUEST);
    guest.set_candidate(HOST);
    auto h = host.poll(0);
    auto g = guest.poll(0);
    require(h && g && !host.direct(0) && !guest.direct(0), "start on relay");
    require(!guest.receive(RELAY, bytes(*h), 1), "foreign probe source rejected");
    auto echo = guest.receive(HOST, bytes(*h), 1);
    require(echo && echo->size == h->size, "direct response matches probe size");
    host.receive(GUEST, bytes(*echo), 2);
    require(!host.direct(2) && !guest.direct(2), "one proven direction is not enough");
    echo = host.receive(GUEST, bytes(*g), 3);
    require(echo.has_value(), "return direction echoed");
    guest.receive(HOST, bytes(*echo), 4);
    require(host.direct(4) && guest.direct(4), "both can switch to direct");

    Datagram data;
    const uint8_t inner[HEADER_BYTES]{};
    require(wrap_session(host_key, inner, data), "same game envelope on both paths");
    require(guest.accepts(HOST, bytes(data)) && !guest.accepts(GUEST, bytes(data)), "candidate identity checked");
    require(!host.direct(PATH_LOST_MS + 4) && !guest.direct(PATH_LOST_MS + 4), "lost direct path expires");
    guest.receive(HOST, bytes(*echo), PATH_LOST_MS + 5);
    require(!guest.direct(PATH_LOST_MS + 5), "old Echo cannot restore direct path");
    guest.set_candidate(GUEST);
    require(!guest.accepts(HOST, bytes(data)) && !guest.direct(PATH_LOST_MS + 5), "old candidate revoked");
}

void client() {
    RelayClient client(RELAY);
    require(client.create(0) && !client.create(0), "one pending create");
    auto tx = client.poll(0);
    RelayControl intent;
    require(tx && decode_relay_control(bytes(*tx), intent) && intent.action == RelayAction::Create,
            "initial create");
    RelayControl challenge;
    challenge.kind = RelayKind::Challenge;
    challenge.action = intent.action;
    challenge.request = intent.request;
    challenge.challenge[0] = 11;
    Datagram d;
    require(encode_relay_control(challenge, d), "challenge encodes");
    client.receive(HOST, bytes(d), 1);
    require(!client.poll(1), "foreign relay ignored");
    client.receive(RELAY, bytes(d), 2);
    tx = client.poll(2);
    RelayControl proof;
    require(tx && decode_relay_control(bytes(*tx), proof) && proof.kind == RelayKind::Proof &&
            proof.challenge == challenge.challenge, "echoes address challenge");
    tx = client.poll(152);
    RelayControl retry;
    require(tx && decode_relay_control(bytes(*tx), retry) && retry == intent,
            "lost proof permits fresh challenge for the same request");
    ++challenge.challenge[0];
    require(encode_relay_control(challenge, d), "renewed challenge encodes");
    client.receive(RELAY, bytes(d), 153);
    tx = client.poll(153);
    require(tx && decode_relay_control(bytes(*tx), proof) && proof.kind == RelayKind::Proof &&
            proof.challenge == challenge.challenge, "client uses renewed proof");
    auto grant = challenge;
    grant.kind = RelayKind::Grant;
    grant.code = 12;
    grant.room[0] = 13;
    grant.key[0] = 14;
    grant.peer = GUEST;
    require(encode_relay_control(grant, d), "grant encodes");
    client.receive(RELAY, bytes(d), 154);
    require(client.phase() == RelayPhase::Ready && client.code() == grant.code && client.peer() == GUEST,
            "verified route is usable");
    const uint8_t inner[] = {1, 2};
    require(client.route(inner, d) && client.forward(RELAY, bytes(d)).empty(), "client cannot reflect its Route");
    require(encode_relay_route({RelayKind::Forward, Role::Host, grant.room, grant.key, inner}, d) &&
            client.forward(RELAY, bytes(d)).size() == sizeof(inner) &&
            client.forward(GUEST, bytes(d)).empty(), "forward accepted only from relay with local key");
    require(client.rebind(155) && client.route(inner, d),
            "control verification keeps the existing route available");
}

void fallback_keeps_nonce() {
    RelayClient client(RELAY);
    require(client.create(0), "create before DNS fallback");
    const auto first = client.poll(0);
    RelayControl original;
    require(first && decode_relay_control(bytes(*first), original), "capture request");
    require(!client.poll(10'000) && client.retry_at(HOST, 10'000), "next address after timeout");
    const auto next = client.poll(10'000);
    RelayControl retried;
    require(client.relay() == HOST && next && decode_relay_control(bytes(*next), retried) &&
            retried == original, "same logical create on next address");
}

} // namespace

int main() {
    paths();
    client();
    fallback_keeps_nonce();
    std::puts("net-relay-path: PASS");
}
