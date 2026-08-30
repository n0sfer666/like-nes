#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "fixmath.hpp"

// Текстовый слой бейка атласа: границы полей, целое число пикселей и дробная привязка. ЧЕТВЁРТАЯ
// копия этого слоя в дереве (`framework/input/preset_text.cpp`, `framework/character/profile_text.cpp`,
// `framework/tilemap/map_text.cpp`), и решение то же, что записано там: общий слой пришлось бы
// класть в `framework/core`, то есть править три работающих модуля ради нового потребителя, а
// линковка с любым из них вытянула бы за четырьмя функциями чужую подсистему целиком — реестр падов
// у ввода, контроллер у персонажа, физику у тайлов.
//
// `strtod`/`strtoul` тут не годятся по той же причине, что и там: они зависят от локали, и в ru_RU
// «0.5» прочиталось бы нулём на одной машине и половиной на другой — то есть бейк перестал бы быть
// байт-детерминированным по составу окружения.
namespace framework::graphics {

std::string atlas_trim(const std::string& s);
std::vector<std::string> atlas_split(const std::string& line);
// Пиксель — ЦЕЛОЕ и неотрицательное: половина пикселя в прямоугольнике региона означала бы, что
// нарезка зависит от округления сэмплера. Потолок 65535 — тип полей раскладки.
bool atlas_parse_u16(const std::string& s, uint16_t& out);
bool atlas_parse_fix(const std::string& s, fix32& out);

} // namespace framework::graphics
