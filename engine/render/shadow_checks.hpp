#pragma once

#include <cstdint>

#include "light_pass.hpp"
#include "material_scene.hpp"
#include "slot_pass.hpp"
#include "../material/table.hpp"

struct GpuContext;

// Утверждения о ТЕНЯХ (шаг C гейта 7). Отдельным файлом от `light_checks.cpp` по предмету, а не по
// длине: там свет — источники, число которых берётся из таблицы, — здесь перекрытие, у которого
// свой слот материала, свой буфер в графе и своё поле в строке источника.
namespace lightgold {

// Перекрыватели приходят из ТЕКСТУРНОГО СЛОТА `occlusion`, и набор слота НАМЕРЕННО не совпадает с
// набором нормалей: проход, читающий чужое имя слота, нашёл бы карты и молча дал бы кадр — врозь
// эти два набора различают только счётчики.
void shadows_come_from_slots(GpuContext& gpu, matgold::Scene& scene, const mat::Table& mtable,
                             lightgfx::Pass& pass, slotgfx::Pass& normals,
                             slotgfx::Pass& occluders, uint32_t w, uint32_t h);

// Мягкость — ПОЛЕ строки источника: два набора, отличающиеся только ею, обязаны дать разные кадры.
// Без этого «мягкость из таблицы» неотличимо от константы в шейдере.
void softness_comes_from_the_table(GpuContext& gpu, matgold::Scene& scene,
                                   slotgfx::Pass& normals, slotgfx::Pass& occluders, uint32_t w,
                                   uint32_t h);

} // namespace lightgold
