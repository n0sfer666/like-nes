#pragma once
#include <cstdint>

#include "scene_objects.hpp"

namespace framework::brawl {

// Где на пути стоит платформа: сегмент — вершина, от которой она едет, `direction` — куда (у петли
// всегда вперёд, у маятника меняется на концах), `traveled` — пройденное от этой вершины в px.
// Положение выводится из этих трёх, а не копится шагами: целая арифметика не теряет ничего между
// тиками, и снапшот следования восстанавливает платформу точно, без дрейфа.
struct PathFollow {
    uint8_t segment = 0;
    int8_t direction = 1;
    fix32 traveled;
};

// Сдвиг на `speed` px за тик. Остаток хода на конце сегмента переходит на следующий: на вершине
// платформа не встаёт. Путь из одной точки и нулевая скорость стоят на месте.
void advance_path(const scene::Path& path, PathFollow& f);

// Точка пути, где стоит платформа: середина её прямоугольника `w × h`.
Vec2 path_point(const scene::Path& path, const PathFollow& f);

} // namespace framework::brawl
