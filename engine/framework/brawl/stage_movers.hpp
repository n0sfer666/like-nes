#pragma once
#include <vector>

#include "brawl_step.hpp"
#include "path_follow.hpp"
#include "physics_world.hpp"

namespace framework::brawl {

// Физмир драки (#15): по кинематическому телу `w × h` на каждый `path` уровня, в порядке путей
// уровня — и индекс тела, и ключ берутся из него, а не из порядка вызовов игры. Игра кладёт
// `&world` в `PlatformWorld.scene.world`: контроллер #16 стоит на этих телах и едет с ними опорой.
struct StageMovers {
    explicit StageMovers(const scene::LevelObjects& level);
    // `PlatformWorld.scene.world` держит адрес `world`: копия или перенос оставили бы его висеть.
    StageMovers(const StageMovers&) = delete;
    StageMovers& operator=(const StageMovers&) = delete;

    physics::World world;
    std::vector<PathFollow> follow;
};

// Начало системы 4 («физика, потом персонаж» #16): следование сдвигается, скорость тела ставится
// так, чтобы за шаг оно пришло в точку пути, затем шаг мира. Цель — точка пути, а не прошлая цель:
// погрешность `Δ / dt * dt` одного тика не переходит в следующий. Abort с причиной — уровень с `path`
// без `m`, `m` без уровня или `w.platform`, мир `m` не в сцене контроллера, скорость пути за
// `MAX_SPEED` на этом `dt`: платформы молча стояли бы, отставали или пропускали бойца сквозь себя.
void step_stage_movers(StageMovers* m, const BrawlWorld& w);

} // namespace framework::brawl
