#include "net_lobby.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
using namespace net;
using platform::net::ADDRESS_LOOPBACK;
using platform::net::Endpoint;

void require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "net-session-wire: FAIL %s\n", what);
    std::exit(1);
}

void lobby_codec() {
    LobbyFrame hello;
    hello.attempt[0] = 1;
    Datagram d;
    require(encode_lobby(hello, d) && d.size == LOBBY_BYTES, "encode exact lobby size");
    LobbyFrame decoded;
    require(decode_lobby({d.bytes, d.size}, decoded) && decoded == hello, "round-trip Hello");
    for (size_t n = 0; n < d.size; ++n) {
        require(!decode_lobby({d.bytes, n}, decoded) && decoded == hello, "every prefix rejected atomically");
    }
    require(!decode_lobby({d.bytes, d.size + 1}, decoded), "oversized datagram");
    d.bytes[8 + Token{}.size()] = 1;
    require(!decode_lobby({d.bytes, d.size}, decoded), "unused fields must be zero");

    LobbyFrame offer;
    offer.kind = LobbyKind::Offer;
    offer.attempt[0] = 1;
    offer.proof[0] = 2;
    offer.session[0] = 3;
    offer.host_key[0] = 4;
    offer.guest_key[0] = 5;
    offer.delay = 2;
    offer.match_hash = match_hash(offer.data, offer.delay);
    require(encode_lobby(offer, d) && decode_lobby({d.bytes, d.size}, decoded) && decoded == offer,
            "round-trip Offer");
    d.bytes[d.size - 2] = 3;
    require(!decode_lobby({d.bytes, d.size}, decoded), "delay mutation invalidates match hash");
    offer.guest_key = offer.host_key;
    require(!encode_lobby(offer, d), "distinct directional keys");
}

void session_codec() {
    const Endpoint guest{ADDRESS_LOOPBACK, 4141}, host{ADDRESS_LOOPBACK, 4142};
    SessionKey host_key{guest}, guest_key{host};
    host_key.session[0] = 1;
    host_key.host_key[0] = 2;
    host_key.guest_key[0] = 3;
    guest_key.session = host_key.session;
    guest_key.host_key = host_key.host_key;
    guest_key.guest_key = host_key.guest_key;
    guest_key.local_role = Role::Guest;

    auto sender = std::make_unique<Link>(3);
    auto receiver = std::make_unique<Link>(3);
    uint8_t msg = 42;
    require(sender->send(&msg, 1), "link send");
    sender->pump(1);
    const auto& tx = sender->outbox_at(0);
    Datagram d;
    require(wrap_session(host_key, {tx.bytes, tx.size}, d), "wrap payload");
    auto opened = unwrap_session(guest_key, host, {d.bytes, d.size});
    require(std::ranges::equal(opened, std::span(tx.bytes, tx.size)), "same bytes");
    uint8_t body = 0;
    size_t size = 0;
    require(receiver->receive(opened.data(), opened.size(), &body, 1, &size) == Received::Delivered &&
            body == msg && size == 1, "real Link receives");
    receiver->pump(1);
    const auto& ack = receiver->outbox_at(0);
    require(wrap_session(guest_key, {ack.bytes, ack.size}, d), "wrap ACK");

    auto receive_ack = [&](const Endpoint& source, const Datagram& packet) {
        auto payload = unwrap_session(host_key, source, {packet.bytes, packet.size});
        if (!payload.empty()) sender->receive(payload.data(), payload.size(), &body, 1, &size);
    };
    receive_ack(host, d);
    auto mutation = d;
    mutation.bytes[6] = static_cast<uint8_t>(Role::Host);
    receive_ack(guest, mutation);
    mutation = d;
    mutation.bytes[9] ^= 1;
    receive_ack(guest, mutation);
    mutation = d;
    mutation.bytes[9 + Token{}.size()] ^= 1;
    receive_ack(guest, mutation);
    mutation = d;
    --mutation.size;
    receive_ack(guest, mutation);
    require(sender->unacked() == 1, "invalid packet cannot retire Link's pending input");

    sender->pump(4);
    require(sender->outbox_size() == 1, "lost ACK causes retry");
    const auto& retry = sender->outbox_at(0);
    require(receiver->receive(retry.bytes, retry.size, &body, 1, &size) == Received::Duplicate,
            "retry does not redeliver payload");
    receiver->pump(4);
    require(receiver->outbox_size() == 1, "duplicate re-arms a lost ACK");
    const auto& replacement = receiver->outbox_at(0);
    require(wrap_session(guest_key, {replacement.bytes, replacement.size}, d), "wrap replacement ACK");
    receive_ack(guest, d);
    require(sender->unacked() == 0, "authenticated ACK reaches Link");
}

void entropy() {
    Token first{}, second{};
    require(random_token(first) && random_token(second) && first != second,
            "OS CSPRNG supplies distinct nonzero protocol tokens");
}

void admission() {
    constexpr Endpoint host{ADDRESS_LOOPBACK, 4100}, a{ADDRESS_LOOPBACK, 4101},
                       b{ADDRESS_LOOPBACK, 4102};
    Endpoint mapped;
    require(platform::net::parse_endpoint("[::ffff:127.0.0.1]:4103", &mapped), "mapped source");
    Lobby listener{{}}, guest{{}};
    require(guest.join(host, 0), "join for version refusal");
    const auto hello = guest.poll(0);
    require(hello.has_value(), "Hello sent");
    LobbyFrame f;
    require(decode_lobby({hello->datagram.bytes, hello->datagram.size}, f), "decode Hello");
    auto inject = [&](Endpoint source, uint64_t at) {
        Datagram d;
        require(encode_lobby(f, d), "encode incoming Hello");
        listener.receive(source, {d.bytes, d.size}, at);
    };
    f.version = MATCH_VERSION + 1;
    inject(a, 0);
    auto reply = listener.poll(0);
    require(reply && reply->datagram.size == hello->datagram.size, "one equal-size version reply");
    guest.receive(b, {reply->datagram.bytes, reply->datagram.size}, 0);
    require(guest.phase() == LobbyPhase::Probing, "foreign refusal ignored");
    guest.receive(host, {reply->datagram.bytes, reply->datagram.size}, 0);
    require(guest.refusal() == Refusal::Version, "stable version rejection");
    f.version = MATCH_VERSION;
    inject(b, 1);
    require(!listener.poll(1), "new addresses throttled");
    inject(b, 25);
    reply = listener.poll(25);
    require(reply && reply->to == b && reply->datagram.size == LOBBY_BYTES, "second slot");
    inject(mapped, 50);
    require(!listener.poll(50) && !listener.poll(200), "same IP limited and no unsolicited reply");
}

} // namespace

int main() {
    lobby_codec();
    session_codec();
    entropy();
    admission();
    std::puts("net-session-wire: PASS");
}
