#pragma once
#include <cstdint>

namespace game {

// Одна запись вершинного буфера. Отдельным заголовком от `batch.hpp` (вертикаль 3, шаг B3): тот
// тянет `<webgpu/webgpu.h>`, а инстанс — обычный POD, и headless-гейты, считающие ЧТО именно
// уезжает на видеокарту, не обязаны ради этого поднимать WebGPU.
struct Instance {
    float x, y, w, h;
    float u0, v0, u1, v1;
    float r, g, b, a;
    float rot = 0;   // S9: поворот квада (наклон корабля, вращение частиц)
};

constexpr uint32_t MAX_INSTANCES = 2048;   // S9: + частицы

} // namespace game
