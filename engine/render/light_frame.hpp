#pragma once

#include <cstdint>
#include <vector>

#include "light_pass.hpp"
#include "material_scene.hpp"

struct GpuContext;

namespace lightgold {

// Кадр гейта 7: сцена материалов рисуется в текстуру, а проход освещения — ОТДЕЛЬНЫМ шагом графа
// поверх неё. `pass == nullptr` значит «прохода в графе нет»: тогда наружу уходит ровно та
// текстура, которую нарисовала сцена, и её байты обязаны совпасть с кадром `matgold::render_frame`.
// Это не удобство вызова, а предмет утверждения — выключенный проход не меняет кадр.
std::vector<uint8_t> render_frame(GpuContext& gpu, matgold::Scene& scene, lightgfx::Pass* pass,
                                  uint32_t w, uint32_t h, uint32_t& draws);

} // namespace lightgold
