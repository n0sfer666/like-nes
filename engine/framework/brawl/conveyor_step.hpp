#pragma once
#include "brawl_body.hpp"
#include "depth_floor.hpp"

namespace framework::scene {
struct LevelObjects;
}

namespace framework::brawl {

// Конвейер (спека #25, В8б) — смещение ЭТОГО тика в начале хода, как перенос опорой в шаге 0
// контроллера: стоявший на ленте в конце прошлого тика едет сейчас, потом ходит сам. В `vx` оно не
// пишется — тело в ударе или реакции катится по `vx` прошлого тика, и добавка копилась бы.
//
// Скорость ленты под точкой тела `(x, z)`: в belt это пол, в platform `z` стоящего — линия его ног.
// Прямоугольник полуоткрыт, как секция: ленты встык не везут на общем краю дважды. Из нескольких
// лент под точкой берётся первая по уровню — сумма вышла бы за `CONVEYOR_SPEED_MAX`. Удерживаемое
// захватом тело (`grip.by`) не везёт никакая: его ведёт бросок. Тело в `hitstop` сюда не доходит.
fix32 conveyor_under(const Body& b, const scene::LevelObjects* level);

// Belt: тело на полу сдвигается по `x` через стены и края полосы, упёршееся стоит.
void ride_belt(DepthBody& d, fix32 belt, const DepthFloor& f);

} // namespace framework::brawl
