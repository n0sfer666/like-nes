#pragma once
#include "brawl_body.hpp"
#include "brawl_step.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

// Поводок сим-камеры: тело зажимается в зону вида по `x`, а пока автоскролл едет по `y` — и по
// экранному `y` ног. Тело belt ходит через пол (`move_x`/`move_z`), тело platform — через
// `move_and_slide` контроллера, иначе зона без стен вставила бы его внутрь стены. Скорость по зажатой
// оси гасится, остальная — нет: тело platform в `hitstop` держит отбрасывание в `pos`, и переходы
// секций до поводка уже перенесли его в `Character`. Край, который пока едет по своей оси, и стена не
// пускают тело в зону — `Body::crushed`, исход решает игра; `Character::crushed` остаётся фактом
// контроллера. Стена у тела platform — факт скольжения: бок (`hit_wall`) по `x`, потолок вверх, пол
// вниз; склон под ногами стеной не считается.
struct Leash {
    Vec2 center;
    Vec2 half;
    bool crush_x = false;
    bool push_y = false;
};

void leash_body(Body& b, SectionBody* s, const Leash& l, const BrawlWorld& w);

} // namespace framework::brawl
