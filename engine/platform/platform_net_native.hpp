#pragma once

// Included after the platform's socket headers.
#include "platform_net.hpp"

#include <algorithm>
#include <cstring>

namespace platform::net::detail {

struct SocketAddress {
    sockaddr_storage storage{};
    int size = 0;

    const sockaddr* data() const { return reinterpret_cast<const sockaddr*>(&storage); }
    sockaddr* data() { return reinterpret_cast<sockaddr*>(&storage); }
};

inline SocketAddress to_sockaddr(const Endpoint& e) {
    SocketAddress result;
    if (e.family == AddressFamily::IPv6) {
        sockaddr_in6 a{};
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(e.port);
        a.sin6_scope_id = e.scope_id;
        std::memcpy(&a.sin6_addr, e.address_v6.data(), e.address_v6.size());
        result.size = sizeof(a);
        std::memcpy(&result.storage, &a, sizeof(a));
    } else {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(e.port);
        a.sin_addr.s_addr = htonl(e.address);
        result.size = sizeof(a);
        std::memcpy(&result.storage, &a, sizeof(a));
    }
    return result;
}

inline bool from_sockaddr(const sockaddr* addr, size_t size, Endpoint& out) {
    if (size < sizeof(sockaddr)) return false;
    if (addr->sa_family == AF_INET && size >= sizeof(sockaddr_in)) {
        sockaddr_in a{};
        std::memcpy(&a, addr, sizeof(a));
        out = Endpoint{ntohl(a.sin_addr.s_addr), ntohs(a.sin_port)};
        return true;
    }
    if (addr->sa_family != AF_INET6 || size < sizeof(sockaddr_in6)) return false;
    sockaddr_in6 a{};
    std::memcpy(&a, addr, sizeof(a));
    Endpoint e{0, ntohs(a.sin6_port), AddressFamily::IPv6};
    e.scope_id = a.sin6_scope_id;
    std::memcpy(e.address_v6.data(), &a.sin6_addr, e.address_v6.size());
    out = normalized_endpoint(e);
    return true;
}

inline bool parse_ipv6_bytes(const char* text, std::array<uint8_t, 16>& out) {
    in6_addr addr{};
    if (::inet_pton(AF_INET6, text, &addr) != 1) return false;
    std::memcpy(out.data(), &addr, out.size());
    return true;
}

inline std::string format_ipv6_bytes(const std::array<uint8_t, 16>& bytes) {
    in6_addr addr{};
    std::memcpy(&addr, bytes.data(), bytes.size());
    char buf[INET6_ADDRSTRLEN];
    return ::inet_ntop(AF_INET6, &addr, buf, sizeof(buf)) ? std::string(buf) : std::string{};
}

inline size_t resolve_dns(const std::string& host, uint16_t port, std::span<Endpoint> out) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* list = nullptr;
    // Numeric spellings belong to parse_endpoint. Reject legacy IPv4 aliases as well.
    hints.ai_flags = AI_NUMERICHOST;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &list) == 0) {
        ::freeaddrinfo(list);
        return 0;
    }
    hints.ai_flags = 0;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &list) != 0) return 0;
    size_t count = 0;
    for (const addrinfo* p = list; p && count < out.size(); p = p->ai_next) {
        Endpoint e;
        if (!p->ai_addr || !from_sockaddr(p->ai_addr, static_cast<size_t>(p->ai_addrlen), e)) continue;
        e.port = port;
        const auto used = out.first(count);
        if (std::find(used.begin(), used.end(), e) == used.end()) out[count++] = e;
    }
    ::freeaddrinfo(list);
    return count;
}

} // namespace platform::net::detail
