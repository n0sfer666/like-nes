#pragma once
#include <string>

namespace framework::input {

// Файл профиля игрока: у P1 — сам base (прежний файл не теряется), у игрока N ≥ 2 — суффикс `-pN`
// перед расширением, `controls.txt` → `controls-p2.txt`. Игрок вне диапазона — пустая строка:
// путь «на всякий случай» записал бы чужой профиль поверх файла P1.
std::string profile_file(const std::string& base, int player);

} // namespace framework::input
