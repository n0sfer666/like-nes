#pragma once

namespace platform::net::detail {

template<class Handle, class Value>
bool socket_option(Handle socket, int level, int option, const Value& value) {
    return ::setsockopt(socket, level, option, reinterpret_cast<const char*>(&value),
                        static_cast<int>(sizeof(value))) == 0;
}

template<class Handle>
bool multicast_bind_options(Handle socket, uint32_t interface_address) {
    const int yes = 1;
    in_addr address{};
    address.s_addr = htonl(interface_address);
    if (!socket_option(socket, SOL_SOCKET, SO_REUSEADDR, yes)) return false;
#if defined(__APPLE__)
    if (!socket_option(socket, SOL_SOCKET, SO_REUSEPORT, yes)) return false;
#endif
#if defined(_WIN32)
    const DWORD ttl = 255;
#else
    const unsigned char ttl = 255;
#endif
    return socket_option(socket, IPPROTO_IP, IP_MULTICAST_IF, address) &&
        socket_option(socket, IPPROTO_IP, IP_MULTICAST_TTL, ttl);
}

template<class Handle>
bool multicast_join(Handle socket, uint32_t group, uint32_t interface_address) {
    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = htonl(group);
    membership.imr_interface.s_addr = htonl(interface_address);
    return socket_option(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, membership);
}

} // namespace platform::net::detail
