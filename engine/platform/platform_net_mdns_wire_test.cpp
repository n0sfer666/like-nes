#include "platform_net_mdns_wire.hpp"

#include <cstdio>

int main() {
    using namespace platform::net;
    using namespace platform::net::mdns;
    Message out;
    const auto question = query(SERVICE, PTR);
    if (!parse(std::span(question.bytes).first(question.size), out) || out.response || out.count != 1 ||
        out.records[0].name != SERVICE || out.records[0].type != PTR) return 1;

    const auto goodbye = announce("neon._likenes._udp.local", "neon.local", {ADDRESS_LOOPBACK, 7777}, 0);
    const auto bytes = std::span(goodbye.bytes).first(goodbye.size);
    if (!parse(bytes, out) || !out.response || out.count != 4 || out.records[0].ttl != 0 ||
        service_instance(out.records[0].target) != "neon" || out.records[1].port != 7777 ||
        out.records[3].address.address != ADDRESS_LOOPBACK) return 2;
    for (size_t len = 0; len < bytes.size(); ++len) {
        if (parse(bytes.first(len), out)) return 3;
    }
    if (out.count != 4) return 4;

    std::array<uint8_t, 28> compressed{
        0, 0, 0x84, 0, 0, 0, 0, 1, 0, 0, 0, 0,
        1, 'X', 0, 0, PTR, 0, 1, 0, 0, 0, 120, 0, 3, 1, 'Y', 0
    };
    compressed[24] = 2;
    compressed[25] = 0xc0;
    compressed[26] = 12;
    if (!parse(std::span(compressed).first(27), out) || out.count != 1 ||
        out.records[0].name != "x" || out.records[0].target != "x" || out.records[0].ttl != 120) return 5;
    compressed[26] = 25;
    if (parse(std::span(compressed).first(27), out)) return 6;
    compressed[26] = 28;
    if (parse(compressed, out)) return 7;
    std::puts("platform-mdns-wire: PASS");
}
