#pragma once
#include <string>
#include <vector>

#include "fixmath.hpp"

// Текстовый слой бейка карты: границы полей и число из текста. Третья копия того же слоя в дереве
// (`framework/input/preset_text.cpp`, `framework/character/profile_text.cpp`) — и она осознанная,
// по причине, уже записанной там: общий слой пришлось бы класть в `framework/core`, то есть править
// два работающих модуля ради нового потребителя, а линковка с любым из них вытянула бы за тремя
// функциями чужую подсистему целиком (реестр падов у ввода, контроллер у персонажа).
//
// `strtod` тут не годится по той же причине, что и там: он зависит от локали, и в ru_RU «0.5»
// прочиталось бы нулём на одной машине и половиной на другой — то есть бейк перестал бы быть
// байт-детерминированным по составу окружения.
namespace framework::tilemap {

std::string map_trim(const std::string& s);
std::vector<std::string> map_split(const std::string& line);
bool map_parse_fix(const std::string& s, fix32& out);

} // namespace framework::tilemap
