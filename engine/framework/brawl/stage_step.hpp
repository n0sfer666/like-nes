#pragma once
#include <span>

#include "body_pool.hpp"
#include "brawl_step.hpp"
#include "seat.hpp"
#include "stage_camera.hpp"
#include "stage_sections.hpp"

namespace framework::brawl {

// Шестая система тика (спека #25, «Порядок систем»): центр — середина охвата живых игроков по `x`,
// затем кламп по запертой арене и `bounds`, затем поводок — живые игроки зажимаются в зону
// `VIEW_ZONE`, скорость по зажатой оси обнуляется, как у края полосы. В belt `center.y` не следует
// ни за кем: экранный `y` тела — это `z − y`, и камера, едущая за ним, качалась бы на каждом прыжке.
// После platform-секции он стоит, где его оставило следование: своей цели по `y` у belt нет.
// Живых нет — камера стоит целиком, без клампа: иначе смена `arena` без игроков сдвигала бы вид.
// `w.level == nullptr` — уровень без объектов, клампа нет. Поводок ходит через пол `w.floor`: зона
// камеры не знает стен, и тело, зажатое в обход, оказалось бы внутри стены или за краем полосы.
// В platform-секции (по `center.x`, нужен `w.platform`) `center.y` следует за серединой экранных `y`
// ног с мёртвой зоной `dead_y`. Автоскролл (`ride_scroll`) — между следованием и клампом. Тело,
// которое поводок вдвинул в секцию другого режима, сразу судит переход (`cross_section`); отказ
// возвращает его на край, и пока край едет по `x`, это `crushed`.
void step_camera(StageCamera& camera, BodyPool& pool, const Seats& seats, StageSections& sections,
                 const BrawlWorld& w);

// Вход новых тел → тела → переходы секций → камера → удары (спека #25, системы 4–7).
void step_stage(BodyPool& pool, std::span<const Command> commands, const BrawlWorld& w, StageCamera& camera,
                const Seats& seats, StageSections& sections, HitEvents& events);

} // namespace framework::brawl
