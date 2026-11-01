#pragma once
#include <span>
#include <vector>

#include "path_follow.hpp"
#include "physics_world.hpp"

namespace framework::kinematic {

// Порядок путей задаёт ключи и индексы тел. Сцена потребителя может держать адрес world,
// поэтому копирование и перенос владельца запрещены.
struct Movers {
    explicit Movers(std::span<const scene::Path> paths);
    Movers(const Movers&) = delete;
    Movers& operator=(const Movers&) = delete;

    physics::World world;
    std::vector<PathFollow> follow;
};

// Пути сохраняют порядок и число элементов конструктора; dt > 0, speed <= MAX_SPEED * dt.
// Скорость ведёт к абсолютной точке пути: округление прошлого шага не накапливается.
void step_movers(Movers& m, std::span<const scene::Path> paths, fix32 dt);

} // namespace framework::kinematic
