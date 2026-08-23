#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "fixmath.hpp"

// Текстовый слой бейка профиля: границы полей и число из текста. Копия того же слоя из
// `framework/input/preset_text.cpp` — и это осознанно, а не по недосмотру. Общий слой пришлось бы
// класть в `framework/core`, то есть править работающий модуль ввода ради нового потребителя;
// линковка `framework_character` с `framework_input` вытянула бы за тремя функциями реестр падов,
// ребайнды и сессии перепривязки. Дерево уже так и устроено: у достижений свой текстовый слой
// (`bake_parse.cpp`), у ввода свой.
//
// `strtod` тут не годится по той же причине, что и там: он зависит от локали, и в ru_RU «0.18»
// прочиталось бы нулём на одной машине и мёртвой зоной на другой — то есть бейк перестал бы быть
// байт-детерминированным по составу окружения.
namespace framework::character {

std::string profile_trim(const std::string& s);
std::vector<std::string> profile_split(const std::string& line);
bool profile_parse_fix(const std::string& s, fix32& out);
bool profile_parse_u32(const std::string& s, uint32_t& out);

} // namespace framework::character
