#include "platform_net.hpp"

#include <cstdio>
#include <utility>

// Жизнь сокета шва platform_net (спека #22, шаг B1): привязка, владение, коды возврата. Через
// этот файл не проходит НИ ОДНОЙ датаграммы — доставку утверждает соседняя цель, и разделены они
// по предмету, а не по длине: «сокет непригоден» и «датаграмма дошла» падают по разным причинам,
// а сваленные в один прогон делили бы первый же отказ.
//
// Объявлен ВНЕ ветки WIN32, как platform_env_test и platform_shmem_test: обе реализации отдают
// «работает» одним и тем же способом, и расхождение между ними молчит до живого прогона.
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

void test_an_empty_queue_is_not_an_error() {
    net::Socket s;
    check(bind_any(s), "a socket binds to an ephemeral port");
    check(s.valid(), "a bound socket is valid");
    check(s.local().port != 0, "the kernel named the ephemeral port");

    unsigned char buf[net::MAX_DATAGRAM];
    net::Endpoint from;
    // Несущее утверждение файла: ноль, а не -1. Реализация, свалившая EWOULDBLOCK в отказ,
    // сделала бы пустую очередь фатальной, и опрос сокета в тике завершал бы сессию.
    check(s.recv_from(buf, sizeof(buf), &from) == 0, "an empty queue reads as zero, not as error");
    check(s.recv_from(buf, sizeof(buf), &from) == 0, "and it stays zero on a second look");

    s.close();
    check(!s.valid(), "a closed socket is not valid");
    check(s.recv_from(buf, sizeof(buf), &from) == -1, "reading a closed socket is an error");
    check(!s.send_to(net::Endpoint{}, buf, 4), "sending on a closed socket fails");
}

// Аргумент вне контракта и непригодный сокет — РАЗНЫЕ коды. Свести их в -1 значило бы, что
// вызывающий, сносящий сессию по отказу сокета, снесёт её на своей же опечатке при здоровом
// сокете; буфер меньше потолка при этом молча терял бы валидные крупные датаграммы.
void test_a_bad_argument_is_not_a_bad_socket() {
    net::Socket s;
    check(bind_any(s), "a socket binds for the argument checks");

    unsigned char buf[net::MAX_DATAGRAM];
    net::Endpoint from;
    check(s.recv_from(nullptr, sizeof(buf), &from) == -2, "a null buffer is an argument error");
    check(s.recv_from(buf, sizeof(buf), nullptr) == -2, "a null sender is an argument error");
    check(s.recv_from(buf, net::MAX_DATAGRAM - 1, &from) == -2, "a short buffer is refused");
    check(s.recv_from(buf, sizeof(buf), &from) == 0, "and the socket itself is untouched");
    check(!s.send_to(loopback_to(s), nullptr, 4), "sending from a null pointer fails");
}

void test_a_socket_is_moved_not_copied() {
    net::Socket a;
    check(bind_any(a), "a socket binds before the move");
    const uint16_t port = a.local().port;

    net::Socket moved(std::move(a));
    check(moved.valid() && moved.local().port == port, "the move carries the bound port");
    check(!a.valid(), "and leaves the source empty");
    // Занятый порт занят: без этого утверждения перемещение, ОСТАВИВШЕЕ дескриптор в источнике,
    // выглядело бы точно так же — до первого двойного закрытия.
    net::Socket other;
    check(!other.open(net::ADDRESS_ANY, port), "the moved-from port is still held by the target");

    net::Socket sink;
    check(bind_any(sink), "a second socket binds to be overwritten");
    sink = std::move(moved);
    check(sink.local().port == port, "move assignment closes its own socket and takes the other");
    check(!sink.open(net::ADDRESS_ANY, 0), "an open socket refuses to be reopened");
}

void test_binding_the_loopback_is_not_binding_everything() {
    net::Socket s;
    check(s.open(net::ADDRESS_LOOPBACK, 0), "a socket binds to the loopback alone");
    check(s.local().address == net::ADDRESS_LOOPBACK, "and the kernel reports that address back");
    net::Socket any;
    check(bind_any(any) && any.local().address == net::ADDRESS_ANY, "any-bound reports any");
}

} // namespace

int main() {
    std::printf("platform net socket lifetime\n");
    test_an_empty_queue_is_not_an_error();
    test_a_bad_argument_is_not_a_bad_socket();
    test_a_socket_is_moved_not_copied();
    test_binding_the_loopback_is_not_binding_everything();
    std::printf("platform-net-socket: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
