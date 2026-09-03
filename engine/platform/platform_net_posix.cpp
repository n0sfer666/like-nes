#include "platform_net.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace platform {
namespace net {
namespace {

sockaddr_in to_sockaddr(const Endpoint& e) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(e.port);
    a.sin_addr.s_addr = htonl(e.address);
    return a;
}

Endpoint from_sockaddr(const sockaddr_in& a) {
    Endpoint e;
    e.address = ntohl(a.sin_addr.s_addr);
    e.port = ntohs(a.sin_port);
    return e;
}

} // namespace

bool Socket::open(uint32_t address, uint16_t port) {
    if (valid()) return false;
    const int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return false;

    // Неблокирующий режим ставится ДО bind: между bind и fcntl сокет уже принимает датаграммы,
    // и первый recv на нём заблокировал бы поток — редко и невоспроизводимо.
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        ::close(fd);
        return false;
    }

    Endpoint want;
    want.address = address;
    want.port = port;
    const sockaddr_in addr = to_sockaddr(want);
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return false;
    }

    // Фактический адрес спрашивается у ядра всегда, а не только при port == 0: так local()
    // остаётся ОТВЕТОМ ОС, а не эхом аргумента, и расхождение между запросом и привязкой видно
    // сразу, а не по чужому пакету, ушедшему не туда.
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &len) != 0) {
        ::close(fd);
        return false;
    }

    native_ = static_cast<intptr_t>(fd);
    local_ = from_sockaddr(bound);
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
    const sockaddr_in addr = to_sockaddr(to);
    const ssize_t sent = ::sendto(static_cast<int>(native_), data, n, 0,
                                  reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
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
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        const ssize_t got = ::recvfrom(static_cast<int>(native_), staging, sizeof(staging), 0,
                                       reinterpret_cast<sockaddr*>(&addr), &len);
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
        std::memcpy(data, staging, n);
        *from = from_sockaddr(addr);
        return static_cast<int>(n);
    }
    return 0;
}

} // namespace net
} // namespace platform
