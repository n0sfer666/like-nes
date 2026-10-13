#pragma once
#include "brawl_body.hpp"
#include "brawl_input.hpp"
#include "platform_world.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

// Тело в platform: `Character` #16 — источник правды, `Body.pos` его зеркало для FSM, ударов и
// отрисовки. Экранный `y` ног — `z − y`. `z` — линия последней опоры, а не замороженное значение
// входа: FSM судит опору по `y = 0`, и тело, стоящее на уступе выше входа, иначе висело бы в
// прыжке. Ниже линии `z` идёт за ногами, `y` не уходит в минус. Скорости — px/тик вверх, у
// контроллера — px/с вниз.
fix32 hull_foot(const PlatformWorld& p);
Vec2 hull_half(const PlatformWorld& p);
void mirror_body(const character::Character& c, fix32 foot, fix32 dt, DepthBody& d);
void adopt_body(const DepthBody& d, fix32 foot, fix32 dt, character::Character& c);

// FSM тикает как в belt; в ударе и реакции контроллер получает нулевой ввод. Скорость, записанная в
// `pos` поверх зеркала (отбрасывание, рывок), переносится в контроллер, отброс вверх — в воздух.
// `belt` — скорость ленты под телом (`conveyor_under`): стоящего она переносит до шага контроллера
// тем же атомарным по оси переносом, что опора (`carry_by`), упёршегося в стену оставляет стоять.
void step_platform(Body& b, const BrawlInput& in, bool held, fix32 belt, SectionBody& s, const PlatformWorld& p);

} // namespace framework::brawl
