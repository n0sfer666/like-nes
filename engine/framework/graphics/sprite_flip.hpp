#pragma once
#include <cstdint>

// Ориентация региона (спека #24, В5б). Одна функция на CPU и её зеркало в шейдере: голден кадра
// сверяет GPU с этим отображением, а не с картинкой, нарисованной от руки.
namespace framework::graphics {

struct Texel {
    uint32_t x = 0;
    uint32_t y = 0;
};

// Выходной пиксель квадратного региона стороной `n` → тексел исходника. Обратные шаги идут в
// обратном порядке: V, затем H, затем транспонирование D.
Texel flip_source(uint8_t flip, uint32_t n, Texel out);

} // namespace framework::graphics
