#pragma once

#include <cstdint>
#include <string>

#include "net_link.hpp"
#include "platform_net.hpp"
#include "platformer_peer_rendezvous.hpp"

// Канал одного пира (спека #22, шаг C): сокет, надёжный слой над ним и знакомство с соседом.
//
// Отдельно от самого пира, потому что ответственность другая и у неё есть собственное имя: здесь
// байты доезжают до соседа, там ввод становится тиком. Пир, державший обе, дорос до 243 строк при
// жёстком пороге 250 — и это не про счётчик, а про то, что дальше в него дописывали бы обе.
namespace platformer::channel {

namespace pnet = platform::net;

// Итераций внешнего цикла до переотправки. Не миллисекунды: у надёжного слоя нет собственных часов,
// и привязка к стенному времени сделала бы прогон зависящим от загруженности машины.
constexpr uint32_t RESEND_AFTER = 6;

struct Channel {
    net::Link link{RESEND_AFTER};
    pnet::Socket socket;
    pnet::Endpoint to;
};

// Порт эфемерный: занятый номер на раннере — это отказ, случающийся раз в сто прогонов, и
// вычислять его из имени роли значило бы менять один класс флейка на другой. Публикуется он
// файлом, потому что запуск ребёнка знает argv, а не порт, который ядро выдаст ребёнку.
inline bool connect(Channel& c, const std::string& mine, const std::string& theirs,
                    int64_t timeout_ms) {
    if (!c.socket.open(pnet::ADDRESS_LOOPBACK, 0)) return false;
    if (!rendezvous::publish(mine, c.socket.local().port)) return false;
    uint16_t peer_port = 0;
    if (!rendezvous::await(theirs, peer_port, timeout_ms)) return false;
    c.to.address = pnet::ADDRESS_LOOPBACK;
    c.to.port = peer_port;
    return true;
}

inline void flush(Channel& c, uint32_t now) {
    c.link.pump(now);
    for (size_t i = 0; i < c.link.outbox_size(); ++i) {
        const net::Datagram& d = c.link.outbox_at(i);
        c.socket.send_to(c.to, d.bytes, d.size);
    }
}

// Одна датаграмма за вызов. Исходов ЧЕТЫРЕ: `1` payload готов, `0` очередь пуста, `-1` — что-то
// пришло, но полезной нагрузкой не стало (дубль, подтверждение, чужой), `-2` — сокет непригоден.
//
// Сведи `1` и `-1` в один `false`, и вызывающий перестал бы отличать «сеть молчит» от «сеть
// работает», а на этом различии стоит и ожидание глухого пира, и его же счётчик простоя. Сведи
// `-2` с «пусто» — и непригодный сокет читался бы как молчащий собеседник: пир крутил бы цикл до
// дедлайна, а гейт объявил бы отказом СХОДИМОСТИ отказ шва. Различать их поимённо требует сам шов
// (`platform_net.hpp`), и требует не из вкуса: у отказа и у тишины разные виновные.
inline int poll(Channel& c, uint8_t* out, size_t cap, size_t* len) {
    uint8_t datagram[pnet::MAX_DATAGRAM];
    pnet::Endpoint from;
    const int n = c.socket.recv_from(datagram, sizeof(datagram), &from);
    if (n < 0) return -2;
    if (n == 0) return 0;
    // Отправитель сверяется с известным адресом соседа. Эфемерный порт петли достаётся нам от ядра,
    // и попасть в него вправе кто угодно: чужие seq отравили бы окно подтверждений и дедупликацию,
    // то есть посторонний молча съел бы реальный ввод — дедуп засчитал бы его принятым, а разошлись
    // бы пиры «по наблюдаемому», без единого слова о причине. Pid в именах файлов рандеву закрывает
    // путаницу ФАЙЛАМИ, но не датаграммами.
    if (from != c.to) return -1;
    return c.link.receive(datagram, static_cast<size_t>(n), out, cap, len) ==
                   net::Received::Delivered
               ? 1
               : -1;
}

} // namespace platformer::channel
