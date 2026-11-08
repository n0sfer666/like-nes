#include "net_lobby.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
using namespace net;
using platform::net::Endpoint;
using platform::net::ADDRESS_LOOPBACK;

constexpr Endpoint HOST{ADDRESS_LOOPBACK, 4000};
constexpr Endpoint GUEST{ADDRESS_LOOPBACK, 4001};
constexpr Endpoint STRANGER{ADDRESS_LOOPBACK, 4002};
constexpr uint64_t GUEST_EPOCH = 30'000;

void require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "net-lobby: FAIL %s\n", what);
    std::exit(1);
}

bool serial_token(Token& token) {
    static uint32_t serial = 0;
    token = {};
    ++serial;
    token[0] = static_cast<uint8_t>(serial);
    token[1] = static_cast<uint8_t>(serial >> 8);
    return true;
}

void inject(Lobby& to, const Endpoint& from, const LobbyFrame& f, uint64_t at) {
    Datagram d;
    require(encode_lobby(f, d), "encode input");
    to.receive(from, {d.bytes, d.size}, at);
}

LobbyFrame read(const LobbySend& out) {
    LobbyFrame f;
    require(decode_lobby({out.datagram.bytes, out.datagram.size}, f), "decode output");
    return f;
}

struct Pair {
    Lobby host{{}, serial_token}, guest{{}, serial_token};
    uint64_t now = 1;
    uint64_t one_way = 0;
    LobbyKind drop = LobbyKind::Reject;
    bool dropped = false;
    uint8_t drop_probes = 0;
    bool replay_start = false;
    std::optional<LobbySend> old_start;

    explicit Pair(uint64_t latency) : one_way(latency) {
        require(guest.join(HOST, now + GUEST_EPOCH), "join");
    }

    void deliver(Lobby& to, const Endpoint& source, const LobbySend& out, uint64_t at) {
        const auto frame = read(out);
        const auto kind = frame.kind;
        if (kind == LobbyKind::Challenge && frame.probe == 1 && frame.round < drop_probes) return;
        if (kind == drop && !dropped) {
            dropped = true;
            return;
        }
        const auto bytes = std::span(out.datagram.bytes, out.datagram.size);
        if (kind != LobbyKind::Hello) to.receive(STRANGER, bytes, at);
        to.receive(source, bytes, at);
        to.receive(source, bytes, at);
        if (kind == LobbyKind::Start) old_start = out;
    }

    void advance() {
        if (auto sent = guest.poll(now + GUEST_EPOCH)) {
            require(sent->to == HOST, "guest destination");
            now += one_way;
            deliver(host, GUEST, *sent, now);
        }
        if (auto sent = host.poll(now)) {
            require(sent->to == GUEST, "host destination");
            now += one_way;
            deliver(guest, HOST, *sent, now + GUEST_EPOCH);
        }
        // One millisecond between event-loop turns. The peer clocks have independent epochs.
        if (replay_start && old_start) {
            auto& d = old_start->datagram;
            const auto phase = guest.phase();
            guest.receive(HOST, {d.bytes, d.size}, now + GUEST_EPOCH);
            require(guest.phase() == phase, "replayed Start cannot advance the new attempt");
        }
        ++now;
    }

    void until(LobbyPhase phase) {
        const uint64_t end = now + 5000;
        while (host.phase() != phase && now < end) advance();
        require(host.phase() == phase, "target phase reached before timeout");
    }

    void run(uint8_t expected_delay) {
        // Ready arriving before any offer cannot select a peer.
        LobbyFrame fake_ready;
        fake_ready.kind = LobbyKind::Ready;
        serial_token(fake_ready.attempt);
        serial_token(fake_ready.session);
        serial_token(fake_ready.proof);
        inject(host, GUEST, fake_ready, now);
        require(host.phase() == LobbyPhase::Waiting, "out-of-order ready ignored");
        host.ready();
        guest.ready();
        until(LobbyPhase::Started);
        require(guest.phase() == LobbyPhase::Started && host.agreement() && guest.agreement(),
                "two agreements");
        require(host.agreement()->hash == guest.agreement()->hash &&
                host.agreement()->key.session == guest.agreement()->key.session &&
                guest.agreement()->delay == expected_delay, "same hash/session and expected RTT delay");
    }
};

void old_attempt_and_deadline() {
    Pair p(0);
    p.run(1);
    require(p.old_start.has_value(), "recorded first Start");
    const auto old = *p.old_start;
    const Token old_session = p.guest.agreement()->key.session;
    p.host.listen();
    require(p.guest.join(HOST, p.now + GUEST_EPOCH), "new generation");
    p.host.ready();
    p.guest.ready();
    p.replay_start = true;
    p.until(LobbyPhase::Started);
    require(p.guest.agreement() && p.guest.agreement()->key.session != old_session, "fresh generation");

    p.host.listen();
    require(p.guest.join(HOST, p.now + GUEST_EPOCH), "next generation");
    const auto hello = p.guest.poll(p.now + GUEST_EPOCH);
    require(hello.has_value(), "initial Hello");
    p.host.receive(GUEST, {hello->datagram.bytes, hello->datagram.size}, p.now);
    auto challenge = p.host.poll(p.now);
    require(challenge.has_value(), "initial Challenge");
    const auto after = p.now + LOBBY_TIMEOUT_MS;
    p.guest.receive(HOST, {old.datagram.bytes, old.datagram.size}, after + GUEST_EPOCH);
    require(!p.guest.poll(after + GUEST_EPOCH) && p.guest.refusal() == Refusal::Timeout,
            "late Start rejected");
    auto echo = read(*challenge);
    echo.kind = LobbyKind::Echo;
    inject(p.host, GUEST, echo, after);
    require(!p.host.poll(after), "late Echo cannot advance expired candidate");
}

void late_ready() {
    Pair p(20);
    p.guest.ready();
    p.until(LobbyPhase::Offered);
    const uint64_t offer_deadline = p.now + LOBBY_READY_TIMEOUT_MS - 21;
    p.advance(); // The guest confirms while the host is still deciding.
    p.now = offer_deadline - 1;
    p.host.ready();
    const auto start = p.host.poll(p.now);
    require(start.has_value() && read(*start).kind == LobbyKind::Start, "Start at end of ready window");
    p.now += p.one_way;
    p.deliver(p.guest, HOST, *start, p.now + GUEST_EPOCH);
    p.until(LobbyPhase::Started);
}

void retry_same_port() {
    Pair p(5);
    p.drop = LobbyKind::Challenge;
    p.advance();
    require(p.dropped && p.host.phase() == LobbyPhase::Waiting, "first attempt was interrupted");
    require(p.guest.join(HOST, p.now + GUEST_EPOCH), "retry on same endpoint");
    p.run(1);
}

} // namespace

int main() {
    Pair(16).run(1);
    Pair(17).run(2);
    Pair(32).run(2);
    Pair(33).run(3);
    Pair lossy(5);
    lossy.drop_probes = 2;
    lossy.run(1);
    for (auto kind : {LobbyKind::Hello, LobbyKind::Challenge, LobbyKind::Echo, LobbyKind::Offer,
                      LobbyKind::Ready, LobbyKind::Start, LobbyKind::Started}) {
        Pair pair(0);
        pair.drop = kind;
        pair.run(1);
        require(pair.dropped, "targeted packet was dropped");
    }
    old_attempt_and_deadline();
    late_ready();
    retry_same_port();
    std::puts("net-lobby: PASS");
}
