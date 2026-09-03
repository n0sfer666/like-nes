#pragma once

#include <cstdint>
#include <vector>

#include "light_pass.hpp"
#include "material_scene.hpp"
#include "slot_pass.hpp"

struct GpuContext;

namespace lightgold {

// Состав графа кадра. Каждое поле — ручка гейта, а не удобство вызова: снятый шаг обязан быть
// отличим по кадру от оставленного, и структура держит ровно те три, которыми проверяется вертикаль.
struct Graph {
    // `nullptr` — прохода освещения в графе НЕТ: наружу уходит ровно та текстура, которую
    // нарисовала сцена, и её байты обязаны совпасть с кадром `matgold::render_frame`.
    lightgfx::Pass* light = nullptr;
    // `nullptr` — нормалей нет: проход берёт плоскую (0,0,1) из своей 1x1. Кадр с картами и кадр с
    // плоскими обязаны различаться, иначе «пришло из слота материала» неотличимо от «дали всем одно».
    slotgfx::Pass* normals = nullptr;
    // `nullptr` — перекрывателей нет: проход берёт «ничего не перекрывает» (0) из своей 1x1, то есть
    // светит ровно так, как светил до появления теней. Тень гасится снятием ЭТОГО шага, а не
    // обнулением мягкости в таблице: у поля `shadow` смысла «тени нет» не бывает.
    slotgfx::Pass* occluders = nullptr;
};

std::vector<uint8_t> render_frame(GpuContext& gpu, matgold::Scene& scene, const Graph& graph,
                                  uint32_t w, uint32_t h, uint32_t& draws);

} // namespace lightgold
