#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "net_link.hpp"
#include "net_wire.hpp"
#include "platform_net.hpp"

// Тот же надёжный слой, но поверх НАСТОЯЩЕГО сокета (спека #22, шаг B2). Инъектор из
// net_link_test проверяет поведение под потерями и не проверяет одного: что датаграмма, собранная
// слоем, вообще проходит через шов. Байт, потерянный на границе Writer → sendto → recvfrom →
// Reader, инъектор не воспроизводит, потому что в нём этой границы нет.
namespace {

namespace pnet = platform::net;

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

constexpr uint32_t MESSAGES = 100;
constexpr uint32_t RESEND_AFTER = 8;
constexpr uint32_t ROUNDS = 4000;

struct Peer {
    Peer() : link(RESEND_AFTER) {}

    net::Link link;
    pnet::Socket socket;
    pnet::Endpoint peer;
    uint32_t queued = 0;
    uint32_t seen_count = 0;
    uint32_t twice = 0;
    bool seen[MESSAGES] = {};
};

void step(Peer& p, uint32_t tick) {
    uint8_t body[8];
    if (p.queued < MESSAGES) {
        net::Writer w(body, sizeof(body));
        w.u32(p.queued);
        w.u32(p.queued * 2654435761u);
        if (p.link.send(body, sizeof(body))) ++p.queued;
    }
    p.link.pump(tick);
    for (size_t i = 0; i < p.link.outbox_size(); ++i) {
        const net::Datagram& d = p.link.outbox_at(i);
        check(p.socket.send_to(p.peer, d.bytes, d.size), "the datagram leaves the socket");
    }
}

bool drain(Peer& p) {
    uint8_t datagram[pnet::MAX_DATAGRAM];
    uint8_t payload[net::MAX_PAYLOAD];
    pnet::Endpoint from;
    bool any = false;
    for (;;) {
        const int n = p.socket.recv_from(datagram, sizeof(datagram), &from);
        if (n <= 0) {
            check(n == 0, "reading an open socket is never an error");
            return any;
        }
        any = true;
        size_t len = 0;
        if (p.link.receive(datagram, static_cast<size_t>(n), payload, sizeof(payload), &len) !=
            net::Received::Delivered)
            continue;
        net::Reader r(payload, len);
        uint32_t id = 0;
        uint32_t salt = 0;
        if (!r.u32(&id) || !r.u32(&salt) || id >= MESSAGES) continue;
        if (salt != id * 2654435761u) continue;
        if (p.seen[id]) {
            ++p.twice;
            continue;
        }
        p.seen[id] = true;
        ++p.seen_count;
    }
}

} // namespace

int main() {
    std::printf("net reliability layer over the real loopback socket\n");
    auto a = std::make_unique<Peer>();
    auto b = std::make_unique<Peer>();
    check(a->socket.open(platform::net::ADDRESS_LOOPBACK, 0) &&
              b->socket.open(platform::net::ADDRESS_LOOPBACK, 0),
          "both peers bind an ephemeral loopback port");
    a->peer.address = pnet::ADDRESS_LOOPBACK;
    a->peer.port = b->socket.local().port;
    b->peer.address = pnet::ADDRESS_LOOPBACK;
    b->peer.port = a->socket.local().port;

    // Такт здесь — итерация цикла, а не миллисекунда: у слоя нет собственных часов, и привязка
    // переотправки к стенному времени сделала бы прогон зависящим от загруженности машины.
    // Сон вставляется только тогда, когда не пришло НИЧЕГО, — иначе прогон упирался бы в него.
    for (uint32_t tick = 0; tick < ROUNDS; ++tick) {
        step(*a, tick);
        step(*b, tick);
        // Оба вызова обязаны состояться: короткое замыкание `||` оставило бы очередь второго
        // пира нетронутой на каждой итерации, где первому что-то пришло.
        const bool got_a = drain(*a);
        const bool got_b = drain(*b);
        const bool got = got_a || got_b;
        if (a->seen_count == MESSAGES && b->seen_count == MESSAGES) break;
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    check(a->seen_count == MESSAGES, "every message reached the first peer");
    check(b->seen_count == MESSAGES, "every message reached the second peer");
    check(a->twice == 0 && b->twice == 0, "and none of them arrived twice");
    check(a->link.foreign() == 0 && b->link.foreign() == 0, "nothing on the wire read as foreign");
    std::printf("  a: sent=%u resent=%u delivered=%u | b: sent=%u resent=%u delivered=%u\n",
                a->link.sent(), a->link.resent(), a->link.delivered(), b->link.sent(),
                b->link.resent(), b->link.delivered());
    std::printf("net-loopback: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
