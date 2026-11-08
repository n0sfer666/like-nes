#include "platform_net.hpp"

#include <cstdio>

// Проверка формата числового адреса и системного разрешения имени.
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

void test_ipv6_roundtrip() {
    net::Endpoint scoped;
    check(net::parse_endpoint("[fe80::abcd%4294967295]:65535", &scoped), "scoped IPv6 parses");
    check(scoped.family == net::AddressFamily::IPv6 && scoped.scope_id == UINT32_MAX &&
              scoped.port == 65535 && scoped.address_v6[0] == 0xfe && scoped.address_v6[15] == 0xcd,
          "scope, port and network-order address survive parsing");
    net::Endpoint copy;
    check(net::parse_endpoint(net::endpoint_text(scoped), &copy) && copy == scoped,
          "printed scoped address parses to the same endpoint");
    --copy.scope_id;
    check(copy != scoped, "scope distinguishes two endpoints on different interfaces");

    check(net::parse_endpoint("[::ffff:127.0.0.1]:7", &copy), "mapped IPv4 parses");
    check(copy == net::Endpoint{net::ADDRESS_LOOPBACK, 7} && net::endpoint_text(copy) == "127.0.0.1:7",
          "mapped address uses the same identity as IPv4");
    check(net::parse_endpoint("[::ffff:7f00:1]:7", &copy) && copy == net::Endpoint{net::ADDRESS_LOOPBACK, 7},
          "hex form of mapped IPv4 uses the same identity");
}

void test_ipv6_refusal_is_atomic() {
    const char* bad[] = {"::1:80", "[::1]", "[::1]:0", "[::1]:65536", "[::1]:080", "[::1]:80x",
                         "[::1]:80:90", "[:::1]:80", "[fe80::1%]:80", "[fe80::1%0]:80",
                         "[fe80::1%01]:80", "[fe80::1%4294967296]:80", "[fe80::1%en0]:80",
                         "[::ffff:127.0.0.1%2]:80", "[::ffff:0177.0.0.1]:80", "[::ffff:0.00.0.1]:80",
                         "[ ::1]:80", "[::1 ]:80", "[1.2.3.4]:80"};
    const net::Endpoint keep{net::ADDRESS_LOOPBACK, 7};
    net::Endpoint result = keep;
    for (const char* text : bad) {
        check(!net::parse_endpoint(text, &result), text);
        check(result == keep, "a rejected string leaves the endpoint intact");
    }
    const char embedded[] = "[::1\0x]:80";
    check(!net::parse_endpoint(std::string(embedded, sizeof(embedded) - 1), &result) && result == keep,
          "embedded NUL does not hide a suffix from the parser");
}

void test_resolver() {
    std::array<net::Endpoint, 4> results;
    const auto count = net::resolve_endpoints("localhost", 7777, results);
    check(count != 0, "system resolves localhost");
    for (size_t i = 0; i < count; ++i) {
        net::Endpoint parsed;
        check(results[i].port == 7777 && net::parse_endpoint(net::endpoint_text(results[i]), &parsed) &&
                  parsed == results[i], "DNS result preserves the requested port and canonical address");
        for (size_t j = 0; j < i; ++j) check(results[i] != results[j], "DNS results are deduplicated");
    }
    check(net::resolve_endpoints("localhost", 7777, std::span(results).first(1)) == 1,
          "resolver respects output capacity");
    check(net::resolve_endpoints("localhost", 0, results) == 0 &&
              net::resolve_endpoints("127.0.0.1", 7777, results) == 0 &&
              net::resolve_endpoints("0177.0.0.1", 7777, results) == 0 &&
              net::resolve_endpoints("localhost:1", 7777, results) == 0,
          "resolver refuses an invalid port and numeric or malformed names");
}

} // namespace

int main() {
    std::printf("platform net address parsing\n");
    test_endpoint_text_roundtrips();
    test_a_broken_address_is_refused();
    test_an_octal_looking_octet_is_refused();
    test_ipv6_roundtrip();
    test_ipv6_refusal_is_atomic();
    test_resolver();
    std::printf("platform-net-address: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
