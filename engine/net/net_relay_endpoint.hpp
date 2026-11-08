#pragma once
#include "net_wire.hpp"
#include "platform_net.hpp"

namespace net::relay_detail {

inline bool unicast(const platform::net::Endpoint& e) {
    using namespace platform::net;
    if (!e.port) return false;
    if (e.family == AddressFamily::IPv4) return e.address != ADDRESS_ANY && (e.address >> 24) < 224;
    return e.family == AddressFamily::IPv6 && e.address_v6 != Endpoint{}.address_v6 && e.address_v6[0] != 0xff;
}

inline bool endpoint(Writer& w, const platform::net::Endpoint& in) {
    using namespace platform::net;
    const auto e = normalized_endpoint(in);
    if (!e.port) {
        const std::array<uint8_t, 23> empty{};
        return e == Endpoint{} && w.bytes(empty.data(), empty.size());
    }
    if (!unicast(e)) return false;
    if (e.family == AddressFamily::IPv4) {
        const std::array<uint8_t, 12> unused{};
        return w.u8(4) && w.u16(e.port) && w.u32(0) &&
            w.u32(e.address) && w.bytes(unused.data(), unused.size());
    }
    return w.u8(6) && w.u16(e.port) && w.u32(e.scope_id) && w.bytes(e.address_v6.data(), e.address_v6.size());
}

inline bool endpoint(Reader& r, platform::net::Endpoint& out) {
    using namespace platform::net;
    Endpoint e;
    uint8_t family = 0;
    if (!r.u8(&family) || !r.u16(&e.port) || !r.u32(&e.scope_id)) return false;
    if (family == 6) {
        e.family = AddressFamily::IPv6;
        if (!e.port || !r.bytes(e.address_v6.data(), e.address_v6.size()) ||
            e.address_v6 == Endpoint{}.address_v6 || normalized_endpoint(e).family != e.family) return false;
    } else {
        std::array<uint8_t, 12> unused{};
        if (!r.u32(&e.address) || !r.bytes(unused.data(), unused.size()) ||
            unused != std::array<uint8_t, 12>{} || e.scope_id != 0) return false;
        if (family == 0 ? e.port != 0 || e.address != ADDRESS_ANY :
            family != 4 || e.port == 0 || e.address == ADDRESS_ANY) return false;
    }
    if (family != 0 && !unicast(e)) return false;
    out = e;
    return true;
}

} // namespace net::relay_detail
