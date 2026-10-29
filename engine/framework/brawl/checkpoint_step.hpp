#pragma once
#include "body_pool.hpp"
#include "level_progress.hpp"
#include "seat_step.hpp"
namespace framework::scene {
struct Checkpoint;
struct LevelObjects;
} // namespace framework::scene

namespace framework::brawl {

// Девятая система тика (спека #25): чекпоинт взят, когда живой (`hp > 0`) игрок присутствующего
// места стоит на опоре с `x` не меньше `x` точки. Истории `x` нет: перепрыгнувший точку берёт её на
// приземлении, а не теряет. Засчитывается только `order` больше взятого, из нескольких за тик —
// наибольший; `order` вне [0, 32767] не берётся. Судится после ударов: выбитый в этом тике не берёт.
// `true` — прогресс сдвинулся в этом тике.
bool step_checkpoints(LevelProgress& progress, const BodyPool& pool, const Seats& seats,
                      const scene::LevelObjects& level);

// Точка респауна — первая в порядке уровня с этим `order`; `NO_CHECKPOINT` и отсутствующий — `nullptr`.
const scene::Checkpoint* find_checkpoint(const scene::LevelObjects& level, int16_t order);

} // namespace framework::brawl
