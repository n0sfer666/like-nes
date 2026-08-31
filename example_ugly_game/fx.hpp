#pragma once
#include <cstdint>

#include "fx_events.hpp"
#include "fx_table.hpp"
#include "instance.hpp"
#include "world.hpp"

namespace game {

// Потолок пула. Раньше частицы жили в `std::vector`, растущем на первом же взрыве босса, — то есть
// куча прямо в кадре, ровно то, что гейт 8 спеки #17 запрещает. Потолок называется числом, потери
// СЧИТАЮТСЯ, и «частиц стало больше, чем влезло» теперь можно спросить, а не заметить глазами.
constexpr uint32_t FX_CAP = 512;

// Запрос следов строит и держит ВЫЗЫВАЮЩИЙ, рядом со своим миром. `world.each(лямбда)` собирал его
// ЗАНОВО на каждом вызове (flecs, `query_delegate_w_ent`: `world.query<Args...>()` прямо в
// конструкторе делегата) — сопоставление термов и таблиц за кадр. В КУЧУ это, вопреки ожиданию, не
// ходило: измерено счётчиком в шве `ecs_os_api` — первый мир с запросом стоит 2939 аллокаций, а
// следующие двести сборок запроса на прогретом мире — ноль, flecs отдаёт их из своих же пулов.
// Так что снято тут лишнее ПОСТРОЕНИЕ, а не ходка в кучу.
//
// Полем `Fx` запрос не спрятать: объект переживает мир (гейт гоняет два мира одним `Fx`), и поле
// держало бы запрос к мёртвому.
using TrailQuery = flecs::query<const Transform, const Velocity>;
TrailQuery make_trail_query(flecs::world& world);

// Частицы шутера НА ФРЕЙМВОРКЕ (вертикаль 3, шаг B3). Класс эмиттера декоративный: частицы боя не
// входят в sim-хеш и никогда не входили — старый `fx.cpp` крутил СВОЙ `float`-генератор, и это
// свойство сохранено типом, а не соглашением.
class Fx {
public:
    Fx();

    void emit(const FxSink& sink);
    void emit_trails(const TrailQuery& trails);
    // Ровно ОДИН тик. Аргумента `dt` больше нет: оба вызывающих передавали `1.0f/60`, то есть
    // величину, у которой не было ни одного второго значения, — а параметр, у которого одно
    // значение, есть приглашение однажды передать второе.
    void update();
    // Кадр отдаётся ИНСТАНСАМИ, а не батчем: `SpriteBatch` тянет WebGPU, и гейт, который считает
    // что именно уезжает на видеокарту, поднимал бы ради этого целый графический стек. Проталкивает
    // их в батч `push_fx` из `draw.cpp` — там же, где живут остальные подачи кадра.
    uint32_t draw(const Atlas& atlas, Instance* out, uint32_t max);
    void clear() { em_.clear(); }

    uint32_t count() const { return em_.count(); }
    uint32_t dropped() const { return em_.dropped(); }
    uint32_t stream() const { return em_.stream(); }
    const framework::graphics::Particle& at(uint32_t i) const { return em_.at(i); }

private:
    // Все четыре буфера ПРИНАДЛЕЖАТ вызывающему — это и есть гейт 8: путь «частицы → спрайты →
    // инстансы» за кадр не трогает кучу ни разу.
    framework::graphics::Particle pool_[FX_CAP];
    framework::graphics::Sprite sprites_[FX_CAP];
    uint64_t keys_[FX_CAP];
    framework::graphics::Batch batches_[MAT_Count];
    framework::graphics::DecorEmitter em_;
};

} // namespace game
