#pragma once

#include "net_wire.hpp"
#include "platformer_observed.hpp"

// `Mark` в байты и обратно (спека #22, шаг C). Словарь наблюдаемых у двух процессов тот же, что у
// двух прогонов в одном, — но между процессами он ходит только байтами.
//
// Поле за полем, а не `memcpy` структуры: рядом с `uint64_t` в `Mark` лежат `bool` и `enum`, то есть
// набивка, содержимое которой не определено. Записав её как есть, пиры разошлись бы по байту, в
// который никто не писал, и гейт назвал бы это расхождением состояний. Тот же довод, что завёл
// проводной формат надёжного слоя и сравнение ввода полями.
//
// Читатель тотальный по той же причине, что и в `net_wire`: файл на входе мог не дописаться —
// ребёнка убили, диск кончился, — и «прочитали половину» обязано читаться как отказ, а не как
// состояние, у которого хвост нулевой.
namespace platformer::mark_io {

// Размер ВЫВЕДЕН из полей, а не выписан с запасом: запас отбирает у `Writer::ok()` единственную его
// работу. Буфер, переживающий лишние тридцать восемь байт, молчит ровно тогда, когда писатель
// разъехался с читателем, — то есть в единственном случае, ради которого проверку и держат.
// Слагаемые идут в порядке `put_mark`: два хеша, пять счётчиков сцены, семь счётчиков работы,
// гравитация, флаг сна, две битовые маски и герой.
constexpr size_t BYTES = 2 * 8 + 5 * 4 + 7 * 8 + 2 * 4 + 1 + 2 * 8 + (9 * 4 + 1);

inline void put_counters(net::Writer& w, const ph::WorkCounters& c) {
    w.u64(c.broad_candidates);
    w.u64(c.pairs);
    w.u64(c.narrow_checks);
    w.u64(c.recalled);
    w.u64(c.velocity_projections);
    w.u64(c.position_projections);
    w.u64(c.active_bodies);
}

inline bool get_counters(net::Reader& r, ph::WorkCounters& c) {
    return r.u64(&c.broad_candidates) && r.u64(&c.pairs) && r.u64(&c.narrow_checks) &&
           r.u64(&c.recalled) && r.u64(&c.velocity_projections) &&
           r.u64(&c.position_projections) && r.u64(&c.active_bodies);
}

// Флаги пакуются в байт, а не пишутся по одному: `bool` на проводе — это байт, у которого
// осмыслен один бит, и семь остальных пришлось бы обещать нулевыми у обеих сторон.
inline uint8_t hero_flags(const ch::Character& h) {
    return static_cast<uint8_t>((h.on_ground ? 1u : 0u) | (h.jump_active ? 2u : 0u) |
                                (h.jump_was_held ? 4u : 0u) | (h.hit_ceiling ? 8u : 0u) |
                                (h.hit_wall ? 16u : 0u) | (h.crushed ? 32u : 0u));
}

inline void put_hero(net::Writer& w, const ch::Character& h) {
    w.u32(static_cast<uint32_t>(h.position.x.raw));
    w.u32(static_cast<uint32_t>(h.position.y.raw));
    w.u32(static_cast<uint32_t>(h.velocity.x.raw));
    w.u32(static_cast<uint32_t>(h.velocity.y.raw));
    w.u32(static_cast<uint32_t>(h.state));
    w.u32(h.coyote_left);
    w.u32(h.buffer_left);
    w.u32(h.ladder_regrab_left);
    w.u32(h.support.index);
    w.u8(hero_flags(h));
}

inline bool get_hero(net::Reader& r, ch::Character& h) {
    uint32_t v[9] = {};
    for (uint32_t& x : v)
        if (!r.u32(&x)) return false;
    uint8_t flags = 0;
    if (!r.u8(&flags)) return false;
    h.position.x = fix32::from_raw(static_cast<int32_t>(v[0]));
    h.position.y = fix32::from_raw(static_cast<int32_t>(v[1]));
    h.velocity.x = fix32::from_raw(static_cast<int32_t>(v[2]));
    h.velocity.y = fix32::from_raw(static_cast<int32_t>(v[3]));
    h.state = static_cast<ch::MoveState>(v[4]);
    h.coyote_left = v[5];
    h.buffer_left = v[6];
    h.ladder_regrab_left = v[7];
    h.support.index = v[8];
    h.on_ground = (flags & 1u) != 0;
    h.jump_active = (flags & 2u) != 0;
    h.jump_was_held = (flags & 4u) != 0;
    h.hit_ceiling = (flags & 8u) != 0;
    h.hit_wall = (flags & 16u) != 0;
    h.crushed = (flags & 32u) != 0;
    return true;
}

inline bool put_mark(net::Writer& w, const Mark& m) {
    const ph::observed::Observed& o = m.world;
    w.u64(o.state);
    w.u64(o.events_hash);
    w.u32(o.bodies);
    w.u32(o.contacts);
    w.u32(o.triggers);
    w.u32(o.recalled);
    w.u32(o.events);
    put_counters(w, o.counters);
    w.u32(static_cast<uint32_t>(o.gravity_x));
    w.u32(static_cast<uint32_t>(o.gravity_y));
    w.u8(o.sleep_enabled ? 1u : 0u);
    w.u64(o.frozen);
    w.u64(o.sleeping);
    put_hero(w, m.hero);
    return w.ok();
}

inline bool get_mark(net::Reader& r, Mark& m) {
    ph::observed::Observed& o = m.world;
    uint8_t sleep_enabled = 0;
    if (!r.u64(&o.state) || !r.u64(&o.events_hash) || !r.u32(&o.bodies) ||
        !r.u32(&o.contacts) || !r.u32(&o.triggers) || !r.u32(&o.recalled) || !r.u32(&o.events))
        return false;
    if (!get_counters(r, o.counters)) return false;
    uint32_t gx = 0;
    uint32_t gy = 0;
    if (!r.u32(&gx) || !r.u32(&gy) || !r.u8(&sleep_enabled)) return false;
    o.gravity_x = static_cast<int32_t>(gx);
    o.gravity_y = static_cast<int32_t>(gy);
    o.sleep_enabled = sleep_enabled != 0;
    if (!r.u64(&o.frozen) || !r.u64(&o.sleeping)) return false;
    return get_hero(r, m.hero);
}

} // namespace platformer::mark_io
