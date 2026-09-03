#pragma once

#include <cstdint>
#include <vector>

#include "world.hpp"

// Полный набор публичных наблюдаемых мира — всё, что игра вправе спросить между шагами.
//
// Заведён затем, чтобы утверждение об откате звучало как «мир НЕОТЛИЧИМ от точки снятия», а не как
// «позиции совпали». Разница не словесная: хеш состояния считается по телам, поэтому снимок,
// потерявший счётчики, буфер событий или раскладку покоя, сверку по хешу проходит молча, а игре
// после отката отдаёт ответы из будущего.
//
// Различие называется ПОИМЁННО (`first_difference`), а не сводится к `bool`: «миры разошлись» и
// «разошлись счётчики отката» — разные сообщения в логе, и второе указывает на поле снимка.
namespace framework::physics::observed {

struct Observed {
    uint64_t state = 0;
    uint64_t events_hash = 0;
    uint32_t bodies = 0;
    uint32_t contacts = 0;
    uint32_t triggers = 0;
    uint32_t recalled = 0;
    uint32_t events = 0;
    WorkCounters counters;
    int32_t gravity_x = 0;
    int32_t gravity_y = 0;
    bool sleep_enabled = true;
    // Битовая маска замерших и спящих — по телу на бит. Раскладка покоя есть ответ мира на вопрос
    // «стоит ли ящик», и снимок, вернувший её из будущего, ошибётся именно в нём.
    uint64_t frozen = 0;
    uint64_t sleeping = 0;
};

inline Observed observe(const World& w) {
    Observed o;
    o.state = w.hash();
    o.events_hash = w.event_hash();
    o.bodies = static_cast<uint32_t>(w.bodies().size());
    o.contacts = w.contact_count();
    o.triggers = w.trigger_count();
    o.recalled = w.recalled_pairs();
    o.events = static_cast<uint32_t>(w.events().size());
    o.counters = w.counters();
    o.gravity_x = w.gravity().x.raw;
    o.gravity_y = w.gravity().y.raw;
    o.sleep_enabled = w.sleep_enabled();
    // Шире 64 тел маска молча теряла бы хвост, поэтому граница объявлена здесь: сцены снимка
    // помещаются в неё с запасом, а та, что не поместится, обязана это заметить.
    const uint32_t n = o.bodies < 64 ? o.bodies : 64;
    for (uint32_t i = 0; i < n; ++i) {
        const BodyId id{i};
        if (w.at_rest(id)) o.frozen |= uint64_t{1} << i;
        if (w.sleeping(id)) o.sleeping |= uint64_t{1} << i;
    }
    return o;
}

// `nullptr` — миры неотличимы. Иначе имя первого разошедшегося наблюдаемого.
inline const char* first_difference(const Observed& a, const Observed& b) {
    if (a.bodies != b.bodies) return "body count";
    if (a.state != b.state) return "state hash";
    if (a.events_hash != b.events_hash) return "event hash";
    if (a.frozen != b.frozen) return "frozen layout";
    if (a.sleeping != b.sleeping) return "sleeping layout";
    if (a.contacts != b.contacts) return "contact count";
    if (a.triggers != b.triggers) return "trigger count";
    if (a.events != b.events) return "event buffer";
    if (a.recalled != b.recalled) return "recalled pairs";
    if (a.counters.broad_candidates != b.counters.broad_candidates) return "broad candidates";
    if (a.counters.pairs != b.counters.pairs) return "pair count";
    if (a.counters.narrow_checks != b.counters.narrow_checks) return "narrow checks";
    if (a.counters.recalled != b.counters.recalled) return "recalled counter";
    if (a.counters.velocity_projections != b.counters.velocity_projections) return "velocity work";
    if (a.counters.position_projections != b.counters.position_projections) return "position work";
    if (a.counters.active_bodies != b.counters.active_bodies) return "active bodies";
    if (a.gravity_x != b.gravity_x || a.gravity_y != b.gravity_y) return "gravity";
    if (a.sleep_enabled != b.sleep_enabled) return "sleep switch";
    return nullptr;
}

} // namespace framework::physics::observed
