#include "platform_net.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

// Шов platform_net (спека #22, шаг B1) на трёх ОС, прогон по петле 127.0.0.1. Как и
// platform_env_test с platform_shmem_test, объявлен ВНЕ ветки WIN32: обе реализации отдают
// «работает» одним и тем же способом, и расхождение между ними молчит до живого прогона.
//
// Чего этот файл НЕ проверяет: доставку по настоящей сети, потери, дубли и перестановки. Это
// предмет надёжного слоя и гейта 3, у которых свой инъектор; петля их не воспроизводит и
// изображать обратное было бы вакуумным гейтом. Разбор адреса и жизнь сокета — у соседних целей.
namespace {

namespace net = platform::net;

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

bool bind_any(net::Socket& s) { return s.open(net::ADDRESS_ANY, 0); }

net::Endpoint loopback_to(const net::Socket& s) {
    net::Endpoint e;
    e.address = net::ADDRESS_LOOPBACK;
    e.port = s.local().port;
    return e;
}

// Петля обычно отдаёт датаграмму немедленно, но «обычно» — не утверждение: планировщик вправе
// увести поток между sendto и recvfrom. Потолок в две секунды берётся один раз и на всё.
int wait_for(net::Socket& s, void* buf, size_t cap, net::Endpoint* from) {
    for (int i = 0; i < 200; ++i) {
        const int n = s.recv_from(buf, cap, from);
        if (n != 0) return n;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}

void test_a_datagram_walks_the_loopback() {
    net::Socket a;
    net::Socket b;
    check(bind_any(a) && bind_any(b), "two sockets bind");
    check(a.local().port != b.local().port, "the kernel gave them different ports");

    const net::Endpoint to = loopback_to(b);
    unsigned char payload[16];
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<unsigned char>(i * 7 + 1);
    check(a.send_to(to, payload, sizeof(payload)), "the datagram is accepted for sending");

    unsigned char got[net::MAX_DATAGRAM];
    net::Endpoint from;
    const int n = wait_for(b, got, sizeof(got), &from);
    check(n == static_cast<int>(sizeof(payload)), "the datagram arrives whole");
    check(n > 0 && std::memcmp(got, payload, sizeof(payload)) == 0, "the bytes are the sent ones");
    // Отправитель привязан к ADDRESS_ANY, поэтому его собственный local().address — ноль, а
    // адрес, который видит получатель, — петля. Сверять их напрямую значило бы утверждать
    // неверное и получить зелёный гейт на реализации, которая теряет адрес отправителя.
    check(from.port == a.local().port, "the sender port is the one that sent");
    check(from.address == net::ADDRESS_LOOPBACK, "the sender address is the loopback");
    check(b.recv_from(got, sizeof(got), &from) == 0, "the queue is drained after the read");
}

void test_the_size_ceiling_is_the_seams_own() {
    net::Socket a;
    net::Socket b;
    check(bind_any(a) && bind_any(b), "two sockets bind for the size check");
    const net::Endpoint to = loopback_to(b);

    static unsigned char big[net::MAX_DATAGRAM + 1];
    for (size_t i = 0; i < sizeof(big); ++i) big[i] = static_cast<unsigned char>(i & 0xFF);
    check(!a.send_to(to, big, net::MAX_DATAGRAM + 1), "a datagram above the ceiling is refused");
    check(!a.send_to(to, big, 0), "an empty datagram is refused");
    check(a.send_to(to, big, net::MAX_DATAGRAM), "a datagram at the ceiling is accepted");

    static unsigned char got[net::MAX_DATAGRAM];
    net::Endpoint from;
    const int n = wait_for(b, got, sizeof(got), &from);
    check(n == static_cast<int>(net::MAX_DATAGRAM), "the largest datagram arrives whole");
    check(n > 0 && std::memcmp(got, big, net::MAX_DATAGRAM) == 0, "and its bytes survive");
}

// Ветки, которые глотают чужую датаграмму, ЧУЖИМ отправителем и исполняются: свой send_to их не
// достигает по построению. Без send_raw «три расхождения ОС сведены к одному поведению» было бы
// утверждением о комментарии — код под ним не прогонялся бы ни разу.
void test_a_foreign_datagram_is_swallowed_whole() {
    net::Socket a;
    net::Socket b;
    check(bind_any(a) && bind_any(b), "two sockets bind for the foreign datagram");
    const net::Endpoint to = loopback_to(b);

    static unsigned char big[net::MAX_DATAGRAM + 64];
    std::memset(big, 0xAB, sizeof(big));
    check(a.send_raw(to, big, 0), "a zero-length datagram leaves the raw path");
    check(a.send_raw(to, big, sizeof(big)), "an oversized datagram leaves the raw path");

    unsigned char honest[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    check(a.send_to(to, honest, sizeof(honest)), "an honest datagram follows them");

    static unsigned char got[net::MAX_DATAGRAM];
    net::Endpoint from;
    const int n = wait_for(b, got, sizeof(got), &from);
    check(n == static_cast<int>(sizeof(honest)), "the honest datagram is what the reader gets");
    check(n > 0 && std::memcmp(got, honest, sizeof(honest)) == 0, "and neither foreign one is");
}

// Бюджет отбрасываний наблюдаем: за один вызов шов выбросит не больше DISCARD_BUDGET чужих
// датаграмм и вернёт ноль, даже если очередь не пуста. Реализация с циклом без потолка отдала бы
// полезную датаграмму ПЕРВЫМ же вызовом — то есть позволила бы чужому отправителю задавать время
// нашего вызова.
void test_the_discard_budget_bounds_one_call() {
    net::Socket a;
    net::Socket b;
    check(bind_any(a) && bind_any(b), "two sockets bind for the budget check");
    const net::Endpoint to = loopback_to(b);

    unsigned char junk[4] = {0, 0, 0, 0};
    for (int i = 0; i < net::DISCARD_BUDGET + 4; ++i) check(a.send_raw(to, junk, 0), "junk leaves");
    unsigned char honest[8] = {9, 8, 7, 6, 5, 4, 3, 2};
    check(a.send_to(to, honest, sizeof(honest)), "the honest datagram is queued behind the junk");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    static unsigned char got[net::MAX_DATAGRAM];
    net::Endpoint from;
    check(b.recv_from(got, sizeof(got), &from) == 0, "one call gives up before draining the junk");
    const int n = wait_for(b, got, sizeof(got), &from);
    check(n == static_cast<int>(sizeof(honest)), "the next calls still reach the honest datagram");
}

} // namespace

int main() {
    std::printf("platform net seam over the loopback\n");
    test_a_datagram_walks_the_loopback();
    test_the_size_ceiling_is_the_seams_own();
    test_a_foreign_datagram_is_swallowed_whole();
    test_the_discard_budget_bounds_one_call();
    std::printf("platform-net: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
