#include "platform_net.hpp"

#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

// SIO_UDP_CONNRESET объявляется САМ, а не берётся из заголовка. Не вкус: прогон 33731818317 на
// windows-latest (MSVC 14.51) отбил `#include <mstcpip.h>` ошибкой C2065 на этом самом имени —
// в этом SDK макрос живёт не там, куда его кладёт документация, и угадывать заголовок значит
// узнавать ответ раз в двадцать минут на единственной ОС, которой нет локально.
//
// Значение задано протоколом, а не заголовком: это вендорный IOCTL номер 12, и `_WSAIOW` —
// собственный макрос winsock2.h. Объявление СТОРОЖЁВОЕ: SDK, где имя есть, определит его сам, и
// наша ветка не сработает вовсе.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#include "platform_net_native.hpp"

namespace platform {
namespace net {
namespace {

// Winsock требует инициализации ПРОЦЕССА, у POSIX такого шага нет вовсе. Вызов спрятан сюда, а
// не вынесен наружу отдельной startup()-функцией по одной причине: забытый вызов дал бы шов,
// который работает на двух ОС из трёх, — ровно тот класс расхождения, ради которого шов и
// заведён. Локальная статика инициализируется потокобезопасно (C++11), лениво и один раз.
//
// WSACleanup парного вызова не имеет намеренно: снимать инициализацию можно только последним
// закрытым сокетом, а счётчик сокетов процесса шов не ведёт. Ресурс живёт до конца процесса,
// как и у любого другого глобального синглтона рантайма.
bool winsock_ready() {
    static const bool ok = []() {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ok;
}

SOCKET handle_of(intptr_t native) { return static_cast<SOCKET>(native); }

} // namespace

namespace detail {

bool parse_ipv6(const char* text, std::array<uint8_t, 16>& out) { return parse_ipv6_bytes(text, out); }

std::string format_ipv6(const std::array<uint8_t, 16>& bytes) { return format_ipv6_bytes(bytes); }

size_t resolve_host(const std::string& host, uint16_t port, std::span<Endpoint> out) {
    return winsock_ready() ? resolve_dns(host, port, out) : 0;
}

} // namespace detail

bool Socket::open(const Endpoint& endpoint) {
    if (valid() || !winsock_ready()) return false;
    const Endpoint want = normalized_endpoint(endpoint);
    const int family = want.family == AddressFamily::IPv6 ? AF_INET6 : AF_INET;
    const SOCKET s = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;

    // Неблокирующий режим — до bind, по той же причине, что в POSIX-реализации: между bind и
    // переключением сокет уже принимает датаграммы.
    u_long nonblocking = 1;
    if (::ioctlsocket(s, FIONBIO, &nonblocking) != 0) {
        ::closesocket(s);
        return false;
    }

    // Отключение SIO_UDP_CONNRESET. Уникальная поломка Windows: если предыдущая датаграмма
    // вызвала у адресата ICMP Port Unreachable, СЛЕДУЮЩИЙ recvfrom на нашем сокете возвращает
    // WSAECONNRESET — то есть чужой закрытый порт делает НАШ сокет «непригодным» по контракту
    // шва. На POSIX для UDP такого не бывает вовсе, и без этих строк пир, поднявшийся на секунду
    // позже, ронял бы соединение ровно на одной ОС из трёх.
    //
    // Результат проверяется: молча проглоченный отказ вернул бы сокет, у которого поведение при
    // чужом закрытом порте не то, что обещает шов, — и разошлось бы это только в бою.
    DWORD unused = 0;
    BOOL report_reset = FALSE;
    if (::WSAIoctl(s, SIO_UDP_CONNRESET, &report_reset, static_cast<DWORD>(sizeof(report_reset)),
                   nullptr, 0, &unused, nullptr, nullptr) == SOCKET_ERROR) {
        ::closesocket(s);
        return false;
    }

    // SO_EXCLUSIVEADDRUSE — тоже свойство одной ОС: на Windows чужой процесс вправе привязаться
    // к УЖЕ занятому UDP-порту через SO_REUSEADDR и получать наши датаграммы, чего POSIX для
    // одинаковых адресов не разрешает. Ставится до bind: после привязки опция не действует.
    BOOL exclusive = TRUE;
    if (::setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive),
                     static_cast<int>(sizeof(exclusive))) != 0) {
        ::closesocket(s);
        return false;
    }

    const int only_v6 = 1;
    if (family == AF_INET6 && ::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
            reinterpret_cast<const char*>(&only_v6), static_cast<int>(sizeof(only_v6))) != 0) {
        ::closesocket(s);
        return false;
    }
    const auto addr = detail::to_sockaddr(want);
    if (::bind(s, addr.data(), addr.size) != 0) {
        ::closesocket(s);
        return false;
    }

    detail::SocketAddress bound;
    int len = static_cast<int>(sizeof(bound.storage));
    if (::getsockname(s, bound.data(), &len) != 0 ||
        !detail::from_sockaddr(bound.data(), static_cast<size_t>(len), local_)) {
        ::closesocket(s);
        return false;
    }

    native_ = static_cast<intptr_t>(s);
    return true;
}

void Socket::close() {
    if (!valid()) return;
    ::closesocket(handle_of(native_));
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
    const int sent = ::sendto(handle_of(native_), static_cast<const char*>(data),
                              static_cast<int>(n), 0, addr.data(), addr.size);
    return sent == static_cast<int>(n);
}

int Socket::recv_from(void* data, size_t cap, Endpoint* from) {
    if (data == nullptr || from == nullptr || cap < MAX_DATAGRAM) return -2;
    if (!valid()) return -1;
    // Промежуточный буфер держится и здесь, хотя WSAEMSGSIZE уже отличает переростка сам:
    // симметрия с POSIX-реализацией стоит одного memcpy, а расхождение в длине принятого —
    // именно то, что этот шов обязан прятать.
    char staging[MAX_DATAGRAM + 1];
    for (int discarded = 0; discarded <= DISCARD_BUDGET; ++discarded) {
        detail::SocketAddress addr;
        int len = static_cast<int>(sizeof(addr.storage));
        const int got = ::recvfrom(handle_of(native_), staging, static_cast<int>(sizeof(staging)),
                                   0, addr.data(), &len);
        if (got == SOCKET_ERROR) {
            const int err = ::WSAGetLastError();
            if (err == WSAEWOULDBLOCK) return 0;
            // Переросток и остаточный ICMP-отказ — не поломка сокета, а свойство очереди:
            // датаграмма выброшена ядром, читать надо дальше.
            if (err == WSAEMSGSIZE || err == WSAECONNRESET) continue;
            return -1;
        }
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
