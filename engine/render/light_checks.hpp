#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "light_pass.hpp"
#include "material_scene.hpp"
#include "normal_pass.hpp"
#include "../light/table.hpp"
#include "../material/table.hpp"

struct GpuContext;

// Портируемая половина гейта 7: утверждения, одинаковые на Metal, lavapipe и WARP, — они и есть
// `--selftest` на трёх ОС. Отдельно от `light_golden_main.cpp`, потому что предмет другой: там
// обвязка прогона и сверка с эталонным PNG владельца, здесь — сами утверждения о проходе.
namespace lightgold {

void check(bool ok, const char* what);
int failures();

bool load_table(const std::string& src, std::vector<uint8_t>& bytes, light::Table& t,
                const char* what);

// Число источников — свойство ДАННЫХ: два набора разной длины обязаны дать разные кадры. Без
// этого утверждения «читаем таблицу» неотличимо от «зашили столько же».
void count_comes_from_data(GpuContext& gpu, matgold::Scene& scene, uint32_t w, uint32_t h);

// Нормали приходят из ТЕКСТУРНОГО СЛОТА материала: набор обязан быть смешанным (есть материалы со
// слотом и есть без), имена совпадать со слотами таблицы, а кадр с картами — отличаться от кадра с
// плоскими. Порознь ни одно из трёх не отличает «читаем слот» от «привязали одну текстуру всем».
void normals_come_from_slots(GpuContext& gpu, matgold::Scene& scene, const mat::Table& mtable,
                             lightgfx::Pass& pass, normgfx::Pass& normals, uint32_t w, uint32_t h);

// Замер стоимости прохода: кадр без него и кадр с ним. Числом, а не утверждением, — время зависит
// от машины, и порог на нём был бы утверждением о раннере (правило «перф-цифра с быстрой машины»).
void report_cost(GpuContext& gpu, matgold::Scene& scene, lightgfx::Pass& pass,
                 normgfx::Pass& normals, uint32_t w, uint32_t h);

} // namespace lightgold
