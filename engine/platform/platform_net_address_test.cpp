#include "platform_net.hpp"

#include <cstdio>

// Разбор и печать адреса — половина шва, у которой нет ни сокета, ни ОС. Отдельной целью она
// стоит потому, что предмет у неё другой: гейт петли утверждает про доставку, этот — про то, какая
// строка вообще считается адресом. Смешанные в один файл, они делили бы прогон, а падать обязаны
// по-разному.
namespace {

namespace net = platform::net;

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

void test_endpoint_text_roundtrips() {
    net::Endpoint e;
    check(net::parse_endpoint("127.0.0.1:7777", &e), "loopback address parses");
    check(e.address == net::ADDRESS_LOOPBACK, "loopback octets land in host byte order");
    check(e.port == 7777, "port parses");
    check(net::endpoint_text(e) == "127.0.0.1:7777", "text of a parsed address is the same text");

    net::Endpoint high;
    check(net::parse_endpoint("255.254.253.252:65535", &high), "the widest address parses");
    check(high.address == 0xfffefdfcu, "the widest octets land unswapped");
    check(net::endpoint_text(high) == "255.254.253.252:65535", "the widest address prints back");

    net::Endpoint zero;
    check(net::parse_endpoint("0.0.0.0:1", &zero), "a single zero octet is still a digit");
    check(zero.address == net::ADDRESS_ANY, "and it is the any-address");
}

// Отказы — половина контракта разбора: парсер, принимающий "1.2.3.4" без порта или октет 256,
// молча отдаёт ЧУЖОЙ адрес, а не ошибку.
void test_a_broken_address_is_refused() {
    net::Endpoint e;
    check(!net::parse_endpoint("127.0.0.1", &e), "an address without a port is refused");
    check(!net::parse_endpoint("127.0.0.1:0", &e), "port zero is refused");
    check(!net::parse_endpoint("127.0.0.1:65536", &e), "a port above the range is refused");
    check(!net::parse_endpoint("127.0.0.256:80", &e), "an octet above the range is refused");
    check(!net::parse_endpoint("127.0.0.1:80x", &e), "a tail after the port is refused");
    check(!net::parse_endpoint(" 127.0.0.1:80", &e), "a leading space is refused");
    check(!net::parse_endpoint("127.0.1:80", &e), "three octets are refused");
    check(!net::parse_endpoint("localhost:80", &e), "a host name is refused");
    check(!net::parse_endpoint("+1.2.3.4:80", &e), "a signed octet is refused");
    check(!net::parse_endpoint("127.0.0.1:80", nullptr), "a null destination is refused");
}

// Ведущий ноль стоит отдельным утверждением, а не строкой в списке выше: он единственный, где
// отказ спасает не от мусора, а от ВТОРОГО написания того же адреса. inet_aton и getaddrinfo
// читают "0177.0.0.1" восьмерично, то есть как 127.0.0.1, десятичный разбор — как 177.0.0.1, и
// фильтр, написанный на одном, охраняет не тот адрес, который откроет системный резолвер.
void test_an_octal_looking_octet_is_refused() {
    net::Endpoint e;
    check(!net::parse_endpoint("0177.0.0.1:80", &e), "a leading zero octet is refused");
    check(!net::parse_endpoint("127.00.0.1:80", &e), "a padded zero octet is refused");
    check(!net::parse_endpoint("127.0.0.1:080", &e), "a leading zero port is refused");
}

} // namespace

int main() {
    std::printf("platform net address parsing\n");
    test_endpoint_text_roundtrips();
    test_a_broken_address_is_refused();
    test_an_octal_looking_octet_is_refused();
    std::printf("platform-net-address: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
