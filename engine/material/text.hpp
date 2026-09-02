#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Лексический слой пекаря материалов. Живёт ЗДЕСЬ, а не берётся у `framework::core::text_fields`,
// хотя тот умеет ровно это: `engine/material` — подсистема, и ребро к слою framework запрещено
// инвариантом 1 спеки #14 (`scripts/tree_invariants.sh deps`). Тот же выбор и по той же причине
// сделан в `engine/achievements/bake_parse.cpp`.
namespace mat::text {

std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);

// Число из текста читается БЕЗ `strtod`: он зависит от локали, и в ru_RU «0.18» стало бы нулём на
// одной машине и восемнадцатью сотыми на другой, то есть бейк перестал бы быть
// байт-детерминированным по составу окружения.
bool parse_float(const std::string& s, float& out);
bool parse_u8(const std::string& s, uint8_t& out);

} // namespace mat::text
