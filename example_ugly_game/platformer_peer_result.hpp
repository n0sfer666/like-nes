#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "platformer_mark_io.hpp"

// Чем один процесс отчитывается перед другим (спека #22, шаг C): наблюдаемое сцены в конце прогона
// плюс счётчики самого прогона.
//
// Счётчики лежат В ТОМ ЖЕ файле, а не рядом, потому что читаются вместе с наблюдаемым и об одном
// прогоне: разъехавшись, они дали бы «хеши сошлись» про один прогон и «откатов не было» про другой.
//
// Файлом, а не stdout ребёнка: разбор чужого вывода означал бы, что печать пира — контракт, и
// первая же добавленная строка ломала бы гейт.
namespace platformer {

// Ноль откатов на прогоне с задержкой и ноль форсированных тиков — это утверждения, а не отладка:
// без них «пиры сошлись» верно и для прогона, в котором откатывать было нечего.
struct PeerStats {
    uint32_t ticks = 0;
    uint32_t rollbacks = 0;
    uint32_t replayed = 0;
    // Тик, сыгранный БЕЗ подтверждения после исчерпанного ожидания. Отказ ждать дальше — заявленный
    // исход («хост, выпавший насовсем»), и он же признак дыры, которая уже не закроется.
    uint32_t forced = 0;
    uint32_t conflicts = 0;
    uint32_t too_deep = 0;
    uint32_t too_far = 0;
    uint32_t resent = 0;
    // Тик, на котором пир оглох, тик, на котором прозрел, и сколько раз внутри окна он не смог
    // шагнуть. Без этих трёх гейт 7 зелен и на прогоне, где молчания не случилось вовсе либо оно
    // ничего не стоило: «сошлись по хешу» тогда сказано про обычный прогон.
    uint32_t deaf_from = 0;
    uint32_t deaf_to = 0;
    uint32_t deaf_stalls = 0;
};

namespace peer_result {

constexpr size_t BYTES = mark_io::BYTES + 44;

inline bool write_file(const std::string& path, const Mark& m, const PeerStats& s) {
    uint8_t buf[BYTES];
    net::Writer w(buf, sizeof(buf));
    if (!mark_io::put_mark(w, m)) return false;
    w.u32(s.ticks);
    w.u32(s.rollbacks);
    w.u32(s.replayed);
    w.u32(s.forced);
    w.u32(s.conflicts);
    w.u32(s.too_deep);
    w.u32(s.too_far);
    w.u32(s.resent);
    w.u32(s.deaf_from);
    w.u32(s.deaf_to);
    w.u32(s.deaf_stalls);
    if (!w.ok()) return false;
    std::FILE* f = platform::open_file(path, "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(buf, 1, w.size(), f) == w.size();
    return std::fclose(f) == 0 && ok;
}

inline bool read_file(const std::string& path, Mark& m, PeerStats& s) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes(path, bytes)) return false;
    net::Reader r(bytes.data(), bytes.size());
    if (!mark_io::get_mark(r, m)) return false;
    return r.u32(&s.ticks) && r.u32(&s.rollbacks) && r.u32(&s.replayed) && r.u32(&s.forced) &&
           r.u32(&s.conflicts) && r.u32(&s.too_deep) && r.u32(&s.too_far) && r.u32(&s.resent) &&
           r.u32(&s.deaf_from) && r.u32(&s.deaf_to) && r.u32(&s.deaf_stalls);
}

} // namespace peer_result
} // namespace platformer
