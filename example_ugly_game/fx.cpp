#include "fx.hpp"

#include "sprite_out.hpp"

namespace game {
namespace {

framework::Vec2 at_of(float x, float y) {
    return {fix32::from_float(x), fix32::from_float(y)};
}

} // namespace

// Зерно то же, что крутил старый `float`-генератор: поток чисел всё равно другой (`fix32` берётся
// из СТАРШИХ разрядов состояния), но менять заодно и зерно значило бы иметь два подозреваемых на
// одно расхождение картинки.
Fx::Fx() : em_(pool_, FX_CAP, fx_table(), FXD_Count, 0x2545f491u) {}

void Fx::emit(const FxSink& sink) {
    for (const FxEvent& e : sink.events) {
        const framework::Vec2 at = at_of(e.x, e.y);
        switch (e.kind) {
            case FX_Fire: em_.burst(FXD_Fire, at, 4); break;
            case FX_EnemyDie: em_.burst(FXD_EnemyDie, at, 16); break;
            case FX_BossHit: em_.burst(FXD_BossHit, at, 5); break;
            case FX_BossDie:
                em_.burst(FXD_BossDieCore, at, 46);
                em_.burst(FXD_BossDieFlash, at, 14);
                break;
            case FX_PlayerHit: em_.burst(FXD_PlayerHit, at, 12); break;
        }
    }
}

TrailQuery make_trail_query(flecs::world& world) {
    return world.query<const Transform, const Velocity>();
}

// Один запрос на оба рода следов, а не два. Два разбили бы обход на «все корабли, потом все пули»,
// то есть поменяли бы ПОРЯДОК розыгрыша генератора эмиттера — и голден сцены сдвинулся бы на
// правке, которая про аллокации.
void Fx::emit_trails(const TrailQuery& trails) {
    trails.each([&](flecs::entity e, const Transform& t, const Velocity&) {
        // Смещение сопла — ЦЕЛОЕ число пикселей и берётся из позиции напрямую: старый код гонял её
        // через `double` только потому, что частица была `float`, и обратный перевод добавлял бы
        // округление там, где его нет.
        if (e.has<Ship>()) {
            em_.burst(FXD_ShipTrail, {t.x - fix32::from_int(52), t.y}, 1);
        } else if (e.has<Bullet>()) {
            em_.burst(FXD_BulletTrail, {t.x - fix32::from_int(16), t.y}, 1);
        }
    });
}

void Fx::update() {
    em_.advance(fix32::from_int(1));
    if (em_.count() > peak_) peak_ = em_.count();
}

uint32_t Fx::draw(const Atlas& atlas, Instance* out, uint32_t max) {
    framework::graphics::SpriteList list(sprites_, keys_, FX_CAP);
    em_.draw(list);
    // `build` зовётся, хотя все частицы сегодня одного слоя и одного материала: порядок отрисовки
    // есть свойство СПИСКА, а не совпадения в таблице, и описание с другим слоем не должно чинить
    // порядок правкой этой строки.
    list.build(batches_, MAT_Count);
    return sprites_to_instances(list, atlas, out, max);
}

} // namespace game
