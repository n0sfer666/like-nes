#pragma once
#include "brawl_step.hpp"
#include "movers.hpp"

namespace framework::brawl {

using StageMovers = kinematic::Movers;

// Начало системы 4 («физика, потом персонаж» #16): следование сдвигается, скорость тела ставится
// так, чтобы за шаг оно пришло в точку пути, затем шаг мира. Цель — точка пути, а не прошлая цель:
// погрешность `Δ / dt * dt` одного тика не переходит в следующий. Abort с причиной — уровень с `path`
// без `m`, `m` без уровня или `w.platform`, мир `m` не в сцене контроллера, скорость пути за
// `MAX_SPEED` на этом `dt`: платформы молча стояли бы, отставали или пропускали бойца сквозь себя.
void step_stage_movers(StageMovers* m, const BrawlWorld& w);

} // namespace framework::brawl
