#include <cstdio>
#include <cstring>
#include <memory>

#include "net_channel.hpp"
#include "net_link.hpp"
#include "net_wire.hpp"

// Гейт 3 спеки #22: потеря, дубль и переупорядочивание не мешают надёжному слою доставить каждое
// сообщение РОВНО ОДИН РАЗ. Инъектор детерминирован, поэтому красный прогон означает дефект, а не
// невезение, и зелёный — что событие действительно случалось (счётчики инъектора проверяются).
namespace {

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

constexpr uint32_t MESSAGES = 200;
constexpr uint32_t RESEND_AFTER = 4;
constexpr uint32_t TICKS = 900;

void test_a_foreign_datagram_never_reaches_the_payload() {
    net::Link link(RESEND_AFTER);
    uint8_t out[net::MAX_PAYLOAD];
    size_t n = 0;
    const uint8_t garbage[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    check(link.receive(garbage, sizeof(garbage), out, sizeof(out), &n) == net::Received::Foreign,
          "a datagram shorter than the header is foreign");

    check(link.send("hi", 2), "a message is queued");
    link.pump(0);
    check(link.outbox_size() == 1, "and one datagram is ready");
    net::Datagram d = link.outbox_at(0);

    net::Datagram bad = d;
    bad.bytes[0] ^= 0xFFu;
    check(link.receive(bad.bytes, bad.size, out, sizeof(out), &n) == net::Received::Foreign,
          "a wrong magic is foreign");
    bad = d;
    bad.bytes[4] = 99;
    check(link.receive(bad.bytes, bad.size, out, sizeof(out), &n) == net::Received::Foreign,
          "a wrong version is foreign");
    bad = d;
    bad.bytes[5] = 0x80u;
    check(link.receive(bad.bytes, bad.size, out, sizeof(out), &n) == net::Received::Foreign,
          "an unknown kind bit is foreign");
    check(link.foreign() == 4, "every one of them was counted");

    // Живучесть: слой, испортивший состояние на чужом пакете, отдал бы этот как чужой тоже.
    check(link.receive(d.bytes, d.size, out, sizeof(out), &n) == net::Received::Delivered,
          "an honest datagram still arrives after the garbage");
    check(n == 2 && std::memcmp(out, "hi", 2) == 0, "and carries its payload");
}

struct Peer {
    explicit Peer(uint32_t seed, const net::ChannelPolicy& p) : link(RESEND_AFTER), out(seed, p) {}

    net::Link link;
    net::Channel out; // канал, по которому УХОДЯТ датаграммы этого пира
    uint32_t queued = 0;
    uint32_t seen_count = 0;
    uint32_t twice = 0;
    bool seen[MESSAGES] = {};
};

void payload_of(uint32_t id, uint8_t* buf) {
    net::Writer w(buf, 8);
    w.u32(id);
    w.u32(id * 2654435761u);
}

void absorb(Peer& self, net::Channel& incoming, uint32_t tick) {
    net::Datagram d;
    uint8_t body[net::MAX_PAYLOAD];
    size_t n = 0;
    while (incoming.take(tick, &d)) {
        if (self.link.receive(d.bytes, d.size, body, sizeof(body), &n) != net::Received::Delivered)
            continue;
        net::Reader r(body, n);
        uint32_t id = 0;
        uint32_t salt = 0;
        if (!r.u32(&id) || !r.u32(&salt) || id >= MESSAGES) continue;
        // Соль сверяется, а не игнорируется: слой, склеивший две нагрузки, отдал бы верный
        // номер с чужим хвостом, и «доставлено ровно один раз» осталось бы правдой.
        if (salt != id * 2654435761u) continue;
        if (self.seen[id]) {
            ++self.twice;
            continue;
        }
        self.seen[id] = true;
        ++self.seen_count;
    }
}

// Один прогон обмена. Возвращает, все ли сообщения дошли к обоим — счётчики читаются снаружи.
void exchange(Peer& a, Peer& b) {
    uint8_t body[8];
    for (uint32_t tick = 0; tick < TICKS; ++tick) {
        for (Peer* p : {&a, &b}) {
            if (p->queued < MESSAGES) {
                payload_of(p->queued, body);
                if (p->link.send(body, sizeof(body))) ++p->queued;
            }
            p->link.pump(tick);
            for (size_t i = 0; i < p->link.outbox_size(); ++i) p->out.offer(p->link.outbox_at(i), tick);
        }
        absorb(a, b.out, tick);
        absorb(b, a.out, tick);
    }
}

void run_gate(const char* label, const net::ChannelPolicy& policy) {
    std::printf("  %s\n", label);
    auto a = std::make_unique<Peer>(0x1234567u, policy);
    auto b = std::make_unique<Peer>(0x89abcdefu, policy);
    exchange(*a, *b);

    check(a->seen_count == MESSAGES, "every message reached the first peer");
    check(b->seen_count == MESSAGES, "every message reached the second peer");
    check(a->twice == 0 && b->twice == 0, "and none of them arrived twice");
    check(a->out.overflowed() == 0 && b->out.overflowed() == 0,
          "the injector never lost a datagram to its own buffer");

    // Половина гейта — доказать, что ловить было что: инъектор без единой потери, без дубля и
    // без перестановки даёт зелёный прогон на слое, у которого нет ни дедупликации, ни повтора.
    check(a->out.dropped() > 0 && b->out.dropped() > 0, "the injector did drop datagrams");
    check(a->out.duplicated() > 0 && b->out.duplicated() > 0, "the injector did duplicate them");
    check(a->out.delayed() > 0 && b->out.delayed() > 0, "the injector did delay some of them");
    check(a->link.duplicates() > 0 && b->link.duplicates() > 0, "and the layer refused repeats");
    check(a->link.reordered() > 0 && b->link.reordered() > 0,
          "a late datagram was delivered anyway, not held back");
    check(a->link.foreign() == 0 && b->link.foreign() == 0, "nothing honest read as foreign");
    std::printf("    drop=%u dup=%u late=%u | resent=%u refused_repeats=%u reordered=%u\n",
                a->out.dropped(), a->out.duplicated(), a->out.delayed(), a->link.resent(),
                a->link.duplicates(), a->link.reordered());
}

// Инъектор обязан быть функцией зерна, а не времени: иначе красный прогон не воспроизводится, и
// гейт превращается в лотерею, которую чинят перезапуском.
void test_the_injector_is_a_function_of_its_seed() {
    net::ChannelPolicy policy;
    policy.loss_percent = 30;
    policy.duplicate_percent = 10;
    policy.reorder_percent = 20;
    policy.reorder_delay = 3;

    uint32_t drops[2] = {0, 0};
    uint32_t dups[2] = {0, 0};
    uint32_t repeats[2] = {0, 0};
    for (int run = 0; run < 2; ++run) {
        auto a = std::make_unique<Peer>(0x1234567u, policy);
        auto b = std::make_unique<Peer>(0x89abcdefu, policy);
        exchange(*a, *b);
        drops[run] = a->out.dropped();
        dups[run] = a->out.duplicated();
        repeats[run] = a->link.duplicates();
    }
    check(drops[0] == drops[1], "the same seed drops the same datagrams");
    check(dups[0] == dups[1], "and duplicates the same ones");
    check(repeats[0] == repeats[1], "so the layer sees the same repeats");
}

} // namespace

int main() {
    std::printf("net reliability layer under loss, duplication and reordering\n");
    test_a_foreign_datagram_never_reaches_the_payload();

    net::ChannelPolicy light;
    light.loss_percent = 10;
    light.duplicate_percent = 10;
    light.reorder_percent = 20;
    light.reorder_delay = 3;
    light.latency = 1;
    run_gate("loss 10%, duplicates 10%, reorder 20% by 3 ticks", light);

    net::ChannelPolicy heavy;
    heavy.loss_percent = 30;
    heavy.duplicate_percent = 20;
    heavy.reorder_percent = 30;
    heavy.reorder_delay = 6;
    heavy.latency = 2;
    run_gate("loss 30%, duplicates 20%, reorder 30% by 6 ticks", heavy);

    test_the_injector_is_a_function_of_its_seed();
    std::printf("net-link: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
