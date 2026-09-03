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

SOCKET handle_of(intptr_t native) { return static_cast<SOCKET>(native); }

} // namespace

bool Socket::open(uint32_t address, uint16_t port) {
    if (valid() || !winsock_ready()) return false;
    const SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

    Endpoint want;
    want.address = address;
    want.port = port;
    const sockaddr_in addr = to_sockaddr(want);
    if (::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::closesocket(s);
        return false;
    }

    sockaddr_in bound{};
    int len = static_cast<int>(sizeof(bound));
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&bound), &len) != 0) {
        ::closesocket(s);
        return false;
    }

    native_ = static_cast<intptr_t>(s);
    local_ = from_sockaddr(bound);
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
    const sockaddr_in addr = to_sockaddr(to);
    const int sent = ::sendto(handle_of(native_), static_cast<const char*>(data),
                              static_cast<int>(n), 0, reinterpret_cast<const sockaddr*>(&addr),
                              static_cast<int>(sizeof(addr)));
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
        sockaddr_in addr{};
        int len = static_cast<int>(sizeof(addr));
        const int got = ::recvfrom(handle_of(native_), staging, static_cast<int>(sizeof(staging)),
                                   0, reinterpret_cast<sockaddr*>(&addr), &len);
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
        std::memcpy(data, staging, n);
        *from = from_sockaddr(addr);
        return static_cast<int>(n);
    }
    return 0;
}

} // namespace net
} // namespace platform
