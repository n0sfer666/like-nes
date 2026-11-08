#include "platform_net.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "platform_net_native.hpp"

namespace platform {
namespace net {

namespace detail {

bool parse_ipv6(const char* text, std::array<uint8_t, 16>& out) { return parse_ipv6_bytes(text, out); }

std::string format_ipv6(const std::array<uint8_t, 16>& bytes) { return format_ipv6_bytes(bytes); }

size_t resolve_host(const std::string& host, uint16_t port, std::span<Endpoint> out) {
    return resolve_dns(host, port, out);
}

} // namespace detail

bool Socket::open(const Endpoint& endpoint) {
    if (valid()) return false;
    const Endpoint want = normalized_endpoint(endpoint);
    const int family = want.family == AddressFamily::IPv6 ? AF_INET6 : AF_INET;
    const int fd = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return false;

    // Неблокирующий режим ставится ДО bind: между bind и fcntl сокет уже принимает датаграммы,
    // и первый recv на нём заблокировал бы поток — редко и невоспроизводимо.
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        ::close(fd);
        return false;
    }

    const int only_v6 = 1;
    if (family == AF_INET6 && ::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &only_v6, sizeof(only_v6)) != 0) {
        ::close(fd);
        return false;
    }
    const auto addr = detail::to_sockaddr(want);
    if (::bind(fd, addr.data(), addr.size) != 0) {
        ::close(fd);
        return false;
    }

    // Фактический адрес спрашивается у ядра всегда, а не только при port == 0: так local()
    // остаётся ОТВЕТОМ ОС, а не эхом аргумента, и расхождение между запросом и привязкой видно
    // сразу, а не по чужому пакету, ушедшему не туда.
    detail::SocketAddress bound;
    socklen_t len = sizeof(bound.storage);
    if (::getsockname(fd, bound.data(), &len) != 0 ||
        !detail::from_sockaddr(bound.data(), static_cast<size_t>(len), local_)) {
        ::close(fd);
        return false;
    }

    native_ = static_cast<intptr_t>(fd);
    return true;
}

void Socket::close() {
    if (!valid()) return;
    ::close(static_cast<int>(native_));
    native_ = -1;
    local_ = Endpoint{};
}

bool Socket::send_to(const Endpoint& to, const void* data, size_t n) {
    if (n == 0 || n > MAX_DATAGRAM) return false;
    return send_raw(to, data, n);
}

bool Socket::send_raw(const Endpoint& to, const void* data, size_t n) {
    if (!valid() || (data == nullptr && n != 0)) return false;
    const Endpoint endpoint = normalized_endpoint(to);
    if (endpoint.family != local_.family) return false;
    const auto addr = detail::to_sockaddr(endpoint);
    const ssize_t sent = ::sendto(static_cast<int>(native_), data, n, 0,
                                  addr.data(), addr.size);
    return sent == static_cast<ssize_t>(n);
}

int Socket::recv_from(void* data, size_t cap, Endpoint* from) {
    if (data == nullptr || from == nullptr || cap < MAX_DATAGRAM) return -2;
    if (!valid()) return -1;
    // Приём идёт в СВОЙ буфер на байт длиннее потолка, а из него копируется наружу. Иначе
    // чужую датаграмму длиной ровно MAX_DATAGRAM нельзя отличить от усечённой: POSIX режет молча
    // (см. контракт в заголовке). Байт лишний ровно один — больше не нужно, вопрос только
    // «влезла или нет».
    unsigned char staging[MAX_DATAGRAM + 1];
    for (int discarded = 0; discarded <= DISCARD_BUDGET; ++discarded) {
        detail::SocketAddress addr;
        socklen_t len = sizeof(addr.storage);
        const ssize_t got = ::recvfrom(static_cast<int>(native_), staging, sizeof(staging), 0,
                                       addr.data(), &len);
        if (got < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
            // EINTR — не отказ сокета, а прерванный сигналом вызов: очередь надо дочитать. Он
            // тратит бюджет наравне с мусором, иначе поток сигналов задавал бы длину вызова так
            // же, как её задавал бы чужой отправитель.
            if (errno == EINTR) continue;
            return -1;
        }
        // Нулевая длина и чужой переросток глотаются, чтение очереди продолжается: вернуть 0
        // значило бы соврать «очередь пуста», оставив в ней настоящие пакеты.
        if (got == 0 || static_cast<size_t>(got) > MAX_DATAGRAM) continue;
        const size_t n = static_cast<size_t>(got);
        if (!detail::from_sockaddr(addr.data(), static_cast<size_t>(len), *from)) continue;
        std::memcpy(data, staging, n);
        return static_cast<int>(n);
    }
    return 0;
}

} // namespace net
} // namespace platform
