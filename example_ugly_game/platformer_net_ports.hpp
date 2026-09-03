#pragma once

#include <cstdint>
#include <string>

#include "platform_net.hpp"
#include "platform_process.hpp"

// Свободные номера портов для гейта прямой адресации (спека #22, гейт 9).
//
// Отдельно от утверждений гейта, потому что предмет другой: здесь номер ищется у ядра, там
// проверяется, что названный сосед находится. В общем файле каждая правка поиска читалась бы как
// правка гейта — та же граница, по которой от гейтов отделена оснастка запуска пары.
namespace platformer::ports {

namespace pnet = platform::net;

// Номера держатся НИЖЕ эфемерного диапазона (Linux по умолчанию 32768-60999, и Windows с Vista
// раздаёт примерно оттуда же): взятый внутри него номер ядро вправе выдать чужому сокету ровно в
// том окне между «спросили» и «ребёнок занял», которое здесь и так неустранимо.
//
// База разведена по pid распорядителя: два прогона рядом (ctest -j, две ветки на раннере) иначе
// брали бы одни и те же номера и мешали бы друг другу ровно там, где гейт проверяет адресацию.
constexpr uint16_t BASE = 24000;
constexpr uint32_t SPREAD = 3900;

inline uint16_t base_for_this_process() {
    return static_cast<uint16_t>(BASE + (platform::process_id() % SPREAD) * 2);
}

// Фиксированный номер здесь неизбежен — в том и смысл формы, что он известен обеим сторонам
// заранее, — а фиксированный номер на раннере это отказ раз в сто прогонов, если его не спросить у
// ядра. Окно между «спросили» и «ребёнок занял» остаётся: сузить его нечем, кроме передачи
// открытого сокета ребёнку, чего шов процессов не умеет ни на одной из трёх ОС. Ошибка в это окно
// даёт код 6 с номером порта — не тишину.
inline bool pick_two(uint16_t& a, uint16_t& b) {
    const uint16_t base = base_for_this_process();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const uint16_t first = static_cast<uint16_t>(base + attempt * 2);
        pnet::Socket sa;
        pnet::Socket sb;
        if (sa.open(pnet::ADDRESS_LOOPBACK, first) &&
            sb.open(pnet::ADDRESS_LOOPBACK, static_cast<uint16_t>(first + 1))) {
            a = first;
            b = static_cast<uint16_t>(first + 1);
            return true;
        }
    }
    return false;
}

inline bool pick_one(uint16_t& a) {
    const uint16_t base = base_for_this_process();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const uint16_t port = static_cast<uint16_t>(base + attempt * 2);
        pnet::Socket s;
        if (s.open(pnet::ADDRESS_LOOPBACK, port)) {
            a = port;
            return true;
        }
    }
    return false;
}

inline std::string at_loopback(uint16_t port) { return "127.0.0.1:" + std::to_string(port); }

} // namespace platformer::ports
