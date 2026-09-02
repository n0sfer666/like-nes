#pragma once
#include <cstdint>

namespace mat {

// Тип параметра задаёт, СКОЛЬКО float'ов он занимает в блоке инстанса, и ничего больше: пайплайн
// про тип не знает, он читает блок целиком (решение 2 спеки #18 — смена значения не порождает
// нового пайплайна).
enum class ParamType : uint8_t { Scalar = 0, Vec2 = 1, Vec4 = 2, Color = 3 };

// Единица — авторская, не машинная (требование «выражение параметров в единицах, понятных
// художнику»). Перевод в сырое число делается ОДИН РАЗ в пекаре: рантайм получает уже радианы и
// уже клампнутую долю, иначе каждый потребитель клампил бы по-своему.
enum class Unit : uint8_t { Raw = 0, Fraction = 1, Pixels = 2, Degrees = 3 };

// Блок параметров инстанса. Восьми float'ов хватает трём эффектам библиотеки с запасом
// (tint 4 + strength 1, color 4 + thickness 1, threshold 1 + edge 1), а размер тут несущий: он
// стоит в раскладке вершинного буфера и в ABI таблицы, поэтому пин — static_assert в table.hpp.
constexpr uint32_t PARAM_BLOCK_FLOATS = 8;

constexpr uint32_t MAX_PARAMS = 8;
constexpr uint32_t MAX_TEXTURES = 4;

uint32_t param_floats(ParamType t);
bool param_type_from_name(const char* name, ParamType& out);
bool unit_from_name(const char* name, Unit& out);

// Клампы и перевод единиц. Значение вне диапазона единицы — не ошибка бейка, а авторская
// небрежность: доля 1.5 значит «полностью», и падать на этом пекарю незачем.
float to_raw(Unit u, float authored);

} // namespace mat
