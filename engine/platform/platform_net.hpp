#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

// Шов №8 платформы (спека #22, решение 4): UDP-датаграммы. Реализации —
// platform_net_posix.cpp (BSD-сокеты) и platform_net_win32.cpp (Winsock2), выбор делает CMake,
// условной компиляции внутри нет. Разбор и печать адреса — чистые функции, живут в общем
// platform_net.cpp: они одинаковы на всех трёх ОС, и дублировать их значило бы завести два
// разных парсера, расходящихся на первом же неверном вводе.
//
// Отдельная цель `platform_net`, а не источник в `platform_core`, — прямое требование спеки
// («сеть отключается целиком: одиночная игра не платит за сетевой слой ни аллокацией, ни
// тиком»). Winsock и ws2_32 приезжают только тому, кто линкует эту цель.
//
// TCP здесь нет и не будет: откатной модели нужна СВЕЖЕСТЬ, а не порядок, и head-of-line
// blocking стоит ровно того, ради чего затевался откат (решение 4 спеки).
namespace platform {
namespace net {

// Потолок датаграммы. 1200 — не круглое число, а консервативный путь под типовой MTU 1280
// (IPv6-минимум) с запасом на заголовки: фрагментированная UDP-датаграмма теряется целиком при
// потере ЛЮБОГО фрагмента, то есть на длине выше MTU надёжный слой начинает бороться с
// собственным транспортом. Отправка длиннее отвергается ШВОМ, а не отдаётся ядру: иначе разница
// между «ОС нарезала» и «ОС отказала» была бы ещё одним расхождением между тремя реализациями.
constexpr size_t MAX_DATAGRAM = 1200;

// Сколько чужих датаграмм один вызов recv_from вправе выбросить, прежде чем вернуть управление.
// Не оптимизация: отбрасывать по кругу нечего мешает НЕДОВЕРЕННОМУ отправителю, а цикл без
// потолка означает, что время нашего вызова задаёт он. Ровно так неблокирующий сокет
// превращается в блокирующий: восемь потоков, шлющих нулевые датаграммы по петле, держали
// recv_from дольше трёх секунд, и один поток тот же дефект не воспроизводил вовсе.
constexpr int DISCARD_BUDGET = 16;

// IPv4 в ХОСТОВОМ порядке байт: сетевой порядок — деталь реализации сокетов, и протекать в
// вызывающий код ему незачем. Наш собственный проводной формат пишется побайтово и от порядка
// байт машины не зависит вовсе.
constexpr uint32_t ADDRESS_ANY = 0u;
constexpr uint32_t ADDRESS_LOOPBACK = 0x7f000001u;

enum class AddressFamily : uint8_t { IPv4, IPv6 };

struct Endpoint {
    uint32_t address = ADDRESS_ANY;
    uint16_t port = 0;
    AddressFamily family = AddressFamily::IPv4;
    std::array<uint8_t, 16> address_v6{};
    uint32_t scope_id = 0;

    bool operator==(const Endpoint& o) const {
        if (port != o.port || family != o.family) return false;
        return family == AddressFamily::IPv4 ? address == o.address :
            address_v6 == o.address_v6 && scope_id == o.scope_id;
    }
};

// IPv4-mapped IPv6 uses the same IPv4 identity as its dotted-decimal spelling.
Endpoint normalized_endpoint(const Endpoint& endpoint);
// Numeric only. Accepts "127.0.0.1:7777" or "[fe80::1%2]:7777" with a numeric scope.
// A rejected string leaves the output intact.
bool parse_endpoint(const std::string& text, Endpoint* out);
std::string endpoint_text(const Endpoint& e);
// Resolve a DNS name with the OS resolver, in its order, dropping duplicate endpoints.
// May block. Call outside the game tick. Returns the number of entries written.
size_t resolve_endpoints(const std::string& host, uint16_t port, std::span<Endpoint> out);
// OS CSPRNG for short protocol nonces and keys. Failure leaves out unchanged.
bool secure_random(std::array<uint8_t, 16>& out);

class Socket {
public:
    Socket() = default;
    ~Socket() { close(); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& o) noexcept { steal(o); }
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) {
            close();
            steal(o);
        }
        return *this;
    }

    // Привязка к address:port, режим НЕБЛОКИРУЮЩИЙ всегда. Адрес — параметр, а не всегда
    // ADDRESS_ANY: порт, принимающий недоверенные датаграммы, незачем открывать на внешних
    // интерфейсах, когда сессия локальная, и «ограничить приём петлёй» обязано быть выразимо. Блокирующего режима у шва нет:
    // тик симуляции не имеет права ждать пакета, а «иногда блокирующий» сокет — это дефект,
    // который воспроизводится раз в сотню прогонов. port == 0 — эфемерный, фактический номер
    // читается из local() после успешного open.
    bool open(uint32_t address, uint16_t port) { return open(Endpoint{address, port}); }
    bool open(const Endpoint& endpoint);
    // IPv4 multicast listener. Interface is a local IPv4 address, or zero for the system default.
    bool open_multicast(uint32_t group, uint16_t port, uint32_t interface_address = ADDRESS_ANY);
    void close();
    bool valid() const { return native_ != -1; }
    const Endpoint& local() const { return local_; }

    // false — датаграмма НЕ ушла (закрытый сокет, нулевая длина, длина выше MAX_DATAGRAM,
    // нулевой указатель, отказ ядра). Успех не означает доставки: за неё отвечает надёжный слой
    // этажом выше, а не UDP.
    bool send_to(const Endpoint& to, const void* data, size_t n);

    // Отправка БЕЗ потолка шва — единственный способ довести до собственного приёмника ту
    // датаграмму, какую шлёт ЧУЖОЙ отправитель: нулевой длины или длиннее MAX_DATAGRAM. Ветки,
    // которые их глотают, иначе не исполняются ни разу, и «три расхождения ОС сведены к одному
    // поведению» осталось бы утверждением о комментарии, а не о коде. Боевой путь — send_to.
    bool send_raw(const Endpoint& to, const void* data, size_t n);

    // >0 — прочитано столько байт; 0 — читать нечего ПРЯМО СЕЙЧАС либо исчерпан
    // DISCARD_BUDGET; -1 — сокет непригоден; -2 — аргументы вне контракта.
    //
    // Различать «пусто» и «ошибка» обязательно: на POSIX это EWOULDBLOCK, на Windows
    // WSAEWOULDBLOCK, и реализация, свалившая их в одно, сделала бы пустую очередь фатальной.
    // Отдельный код -2 нужен по той же причине этажом выше: вызывающий, сносящий сессию по -1,
    // снёс бы её на опечатке в собственных аргументах при здоровом сокете.
    //
    // Контракт аргументов: data и from ненулевые, cap НЕ МЕНЬШЕ MAX_DATAGRAM. Последнее не
    // придирка — буфер меньше потолка означал бы, что валидная крупная датаграмма выбрасывается
    // молча, и надёжный слой видел бы «сеть теряет пакеты» там, где теряет их шов.
    //
    // Две датаграммы шов ГЛОТАЕТ молча и продолжает читать очередь (в пределах бюджета):
    // нулевой длины (её нельзя отличить от пустой очереди возвращаемым значением, а наш слой
    // такую не шлёт) и длиннее MAX_DATAGRAM (чужая). Второе — не педантизм: POSIX такую
    // датаграмму УСЕКАЕТ и отдаёт как валидную, а Windows отвергает целиком с WSAEMSGSIZE, и
    // без этого правила один и тот же чужой пакет означал бы на трёх ОС три разных вещи.
    int recv_from(void* data, size_t cap, Endpoint* from);

private:
    bool open_impl(const Endpoint& endpoint, uint32_t group, uint32_t interface_address);
    void steal(Socket& o) noexcept {
        native_ = o.native_;
        local_ = o.local_;
        o.native_ = -1;
        o.local_ = Endpoint{};
    }

    // Дескриптор POSIX или SOCKET Windows. Хранится как intptr_t, чтобы <winsock2.h> не протёк
    // в заголовок, который включают все, — тот же приём, что у platform_shmem.hpp. -1 негоден
    // на обеих ОС: INVALID_SOCKET есть (SOCKET)(~0), и в intptr_t это ровно -1.
    intptr_t native_ = -1;
    Endpoint local_;
};

} // namespace net
} // namespace platform
