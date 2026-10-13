#pragma once
#include <cstdint>

#include "ent_id.hpp"
#include "fixmath.hpp"

namespace framework::brawl {

// Трос (спека #25, В8в) — маятник с точкой опоры `rope` уровня. Состояние — дуга `arc` от отвеса и
// линейная скорость `speed` по ней, px и px/тик, а не угол и угловая скорость: угол в оборотах на
// длинном тросе квантуется грубее пикселя, и энергия у такого маятника росла до 8 % от одного
// округления. Дуга вправо положительна, `+Y` вниз, как у контроллера #16.
struct RopeState {
    fix32 arc;
    fix32 speed;
    EntId rider;
    uint8_t cooldown = 0;
};

// Досягаемость — от рук (верх корпуса) до конца троса. Раскачка — ускорение вдоль дуги от полного
// отклонения стика, затухание — доля скорости за тик: с ним энергия без ввода не растёт и на
// округлении. Остывание — тики после отпускания, в которые тот же трос не берётся.
inline constexpr fix32 ROPE_REACH = fix32::from_int(16);
inline constexpr fix32 ROPE_PUMP = fix32::from_raw(1024);
inline constexpr int32_t ROPE_DAMPING = 2048;
inline constexpr uint8_t ROPE_COOLDOWN = 12;

fix32 rope_turns(fix32 arc, fix32 length);
// Размах — до горизонтали в обе стороны, четверть окружности дугой.
fix32 rope_cap(fix32 length);
Vec2 rope_end(Vec2 pivot, fix32 length, fix32 arc);
// Единичная касательная в сторону роста дуги.
Vec2 rope_tangent(fix32 arc, fix32 length);

// Полунеявный Эйлер: скорость от тяжести `g` (px/тик²) и раскачки `push`, затем дуга новой
// скоростью, затем затухание. На пределе размаха трос встаёт: дуга зажимается, скорость обнуляется.
void swing_rope(RopeState& r, fix32 length, fix32 g, fix32 push);

} // namespace framework::brawl
