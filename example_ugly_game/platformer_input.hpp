#pragma once
#include "input_types.hpp"
#include "presets.hpp"
#include "state.hpp"

// Раскладка → намерение персонажа. Отдельно от окна, потому что ровно здесь живёт единственное
// место образца, где переворачивается ЗНАК: ось `move_y` положительна ВВЕРХ (так её объявляет
// `assets/input.txt`), а мир считает +Y вниз. Незаписанное соглашение о знаке каждый потребитель
// пишет по-своему — и находит это живым прогоном, а не гейтом.
namespace platformer {

namespace ch = framework::character;

// Индексы, которыми игра спрашивает кадр ввода. Числами не прошиваются: порядок строк в манифесте
// задаёт индексы, и вставленное выше действие молча переставило бы прыжок на чужую кнопку.
struct Binding {
    int jump = -1;
    int move_x = -1;
    int move_y = -1;

    bool valid() const { return jump >= 0 && move_x >= 0 && move_y >= 0; }
};

// Разбор раскладки по ИМЕНАМ. `false` — в пресете нет чего-то из трёх: играть таким управлением
// нечем, и молчаливый `Binding` с индексом -1 читал бы чужую ось.
bool resolve_binding(const framework::input::PresetTable& table, const char* preset, Binding& out);

// «Вниз» — половина хода оси, а не любое отрицательное значение: мёртвая зона стика круговая
// (`assets/input.txt`), и на диагонали разрешённая ось легко даёт -0.3 при честном намерении бежать
// вбок. Проваливаться сквозь площадку от такого — это спуск, которого игрок не просил.
constexpr fix32 DOWN_EDGE = fix32::from_float(-0.5);

ch::MoveInput read_input(const ::input::InputFrame& frame, const Binding& bind);

} // namespace platformer
