#include <cstdio>

#include "net_ack_window.hpp"
#include "net_wire.hpp"

// Проводной формат и окно подтверждений — предмет, у которого нет ни транспорта, ни пира.
// Отдельной целью от гейта 3 (net_link_test) по этой границе: там проверяется поведение слоя под
// инъекцией, здесь — арифметика, которая обязана быть верна ДО того, как в неё придёт сеть.
namespace {

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

void test_the_writer_stops_at_the_edge() {
    uint8_t buf[6];
    net::Writer w(buf, sizeof(buf));
    check(w.u32(1u) && w.u16(2u), "a writer fills its buffer exactly");
    check(w.size() == 6, "and reports the bytes it wrote");
    check(!w.u8(3), "one byte past the end is refused");
    check(!w.ok(), "and the refusal is sticky");
    check(w.size() == 6, "a refused write adds nothing");
}

void test_the_reader_refuses_a_short_datagram() {
    const uint8_t buf[3] = {1, 2, 3};
    net::Reader r(buf, sizeof(buf));
    uint32_t v = 0;
    check(!r.u32(&v), "a four-byte read out of three bytes is refused");
    check(!r.ok(), "and the reader is spoiled for good");
    uint8_t b = 0;
    check(!r.u8(&b), "even a read that would have fit");
    check(r.remaining() == 0, "a spoiled reader has nothing left");
}

void test_the_ack_window_refuses_a_repeat() {
    net::AckWindow w;
    check(w.note(0), "the first sequence is fresh");
    check(!w.note(0), "the same sequence is not fresh twice");
    check(w.note(1) && w.note(3), "later sequences pass");
    check(w.note(2), "a gap filled late still passes");
    check(w.reordered() == 1, "and it is counted as reordered");
    check(!w.note(2), "but not a second time");
    check(w.latest() == 3, "the latest is the most recent, not the last seen");
    // Оборот счётчика: без учёта половины диапазона номер 1 навсегда остался бы «старее» 65535.
    net::AckWindow wrap;
    check(wrap.note(65530), "a high sequence starts the window");
    check(wrap.note(2), "a sequence past the wrap is fresh");
    check(wrap.latest() == 2, "and becomes the latest");
    check(!wrap.note(65530), "the pre-wrap sequence is still remembered");
}

} // namespace

int main() {
    std::printf("net wire format and ack window\n");
    test_the_writer_stops_at_the_edge();
    test_the_reader_refuses_a_short_datagram();
    test_the_ack_window_refuses_a_repeat();
    std::printf("net-wire: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
