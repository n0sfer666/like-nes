#pragma once
#include <span>

#include "body_pool.hpp"
#include "brawl_step.hpp"
#include "seat.hpp"
#include "stage_camera.hpp"

namespace framework::brawl {

// Шестая система тика (спека #25, «Порядок систем»): центр — середина охвата живых игроков по `x`,
// затем кламп по запертой арене и `bounds`, затем поводок — живые игроки зажимаются в зону
// `VIEW_ZONE`, скорость по зажатой оси обнуляется, как у края полосы. В belt `center.y` не следует
// ни за кем: экранный `y` тела — это `z − y`, и камера, едущая за ним, качалась бы на каждом прыжке.
// Живых нет — камера стоит целиком, без клампа: иначе смена `arena` без игроков сдвигала бы вид.
// `w.level == nullptr` — уровень без объектов, клампа нет. Поводок ходит через пол `w.floor`: зона
// камеры не знает стен, и тело, зажатое в обход, оказалось бы внутри стены или за краем полосы.
void step_camera(StageCamera& camera, BodyPool& pool, const Seats& seats, const BrawlWorld& w);

void step_stage(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, StageCamera& camera,
                const Seats& seats, HitEvents& events);

} // namespace framework::brawl
