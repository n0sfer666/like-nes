#pragma once
#include <cstdint>

// Zero-parse секция раскладки тача: общий заголовок секций слоя (`section_format.hpp`), за ним
// строки. Строк имён нет — кнопки названы номером, поэтому блоб строк состоит из одного нуля.
namespace framework::touch {

constexpr uint8_t TOUCH_MAGIC[4] = {'L', 'N', 'T', 'L'};
constexpr uint32_t TOUCH_VERSION = 1;

enum class RowKind : uint32_t { Stick = 0, Button = 1 };

// Стик и кнопки — одной строкой разного вида: у стика заняты только `kind` и `radius_raw`, прочие
// поля обязаны быть нулями, иначе читатель отбивает секцию.
struct TouchRow {
    uint32_t kind;
    uint32_t button;
    uint32_t anchor;
    int32_t dx_raw;       // Q16.16, точки
    int32_t dy_raw;
    int32_t radius_raw;
};
static_assert(sizeof(TouchRow) == 24, "TouchRow layout pinned (zero-parse ABI)");

} // namespace framework::touch
