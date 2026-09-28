#pragma once
#include <cstdint>
#include <cstring>

// Нумерация ЛОГИЧЕСКИХ осей пресета — один ответ на один вопрос, заданный из трёх мест.
//
// У одной оси строк несколько: клавиши, стрелки, стик — альтернативные источники одного движения.
// Номер оси наружу — порядковый номер её ИМЕНИ, а не строки, иначе манифест, которому дописали
// стик, сдвинул бы оси в InputFrame и поменял sim-хеш, ничего не сказав.
//
// Считают этот номер трое: пекарь (`preset_validate.cpp` — предел движка и номер парной оси),
// читатель (`preset_open.cpp` — верхняя граница `pair_axis` чужого бандла) и рантайм
// (`presets.cpp` — слот в InputFrame). Разойдись они хоть на единицу, ось приедет в чужой слот, и
// увидел бы это только голден. Строки лежат у каждого по-своему (сырой блоб, std::string,
// `string_at`), поэтому общей делается не структура, а доступ к имени: `name_of(row)` отдаёт
// C-строку, и сравнение у всех троих буквально одно (аудит #21, ревью A·2).
namespace framework::input {

// Первая ли это строка своей оси: выше по срезу имени ещё не было.
template <class NameOf>
bool axis_row_is_first(uint32_t row, NameOf name_of) {
    const char* name = name_of(row);
    for (uint32_t k = 0; k < row; ++k)
        if (std::strcmp(name_of(k), name) == 0) return false;
    return true;
}

template <class NameOf>
uint32_t logical_axis_count(uint32_t rows, NameOf name_of) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < rows; ++i)
        if (axis_row_is_first(i, name_of)) ++n;
    return n;
}

// Номера ВСЕХ строк за один проход; возвращает число осей. `out` держит НЕ МЕНЬШЕ `rows` элементов,
// и ограничить `rows` потолком формата обязан вызывающий: буфер у него стековый, а число строк
// приходит из чужого бандла. Ответ тот же, что у двух функций ниже,
// но цена другая: `logical_axis_of_row(row)` сам стоит O(row²) сравнений, и обход всех строк с ним
// кубический — на потолках формата (64 строки × 64 пресета) это 5,6 млн `strcmp` по именам до 64
// байт за ОДНО открытие чужого бандла. Здесь квадрат на пресет, то есть в сорок раз дешевле; кто
// спрашивает про все строки сразу — спрашивает этой функцией (аудит #21, ревью A·2).
template <class NameOf>
uint32_t logical_axis_map(uint32_t rows, NameOf name_of, uint32_t* out) {
    uint32_t axes = 0;
    for (uint32_t i = 0; i < rows; ++i) {
        const char* name = name_of(i);
        uint32_t k = 0;
        for (; k < i; ++k)
            if (std::strcmp(name_of(k), name) == 0) break;
        out[i] = k == i ? axes++ : out[k];
    }
    return axes;
}

// Номер оси, которой принадлежит строка. Непервая строка обязана дать тот же номер, что и первая:
// ранний выход по совпадению имени — это оно и есть.
template <class NameOf>
uint32_t logical_axis_of_row(uint32_t row, NameOf name_of) {
    const char* name = name_of(row);
    uint32_t logical = 0;
    for (uint32_t i = 0; i < row; ++i) {
        if (std::strcmp(name_of(i), name) == 0) break;
        if (axis_row_is_first(i, name_of)) ++logical;
    }
    return logical;
}

} // namespace framework::input
