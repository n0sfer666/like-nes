#pragma once
#include <string>
#include <vector>

// Парсер диагностик компилятора (спека #7, гейт 5; MSVC — решение 3 спеки #13). Два формата:
//   path/file.cpp:LINE:COL: severity: message          — clang, gcc
//   path\file.cpp(LINE,COL): severity CODE: message    — MSVC, clang-cl
// → одинаковые структурные записи для build-панели редактора + click-to-open на file:line.
namespace ide::build {

// Нормализованная запись: панель и click-to-open работают от полей, а не от текста строки, и
// потому ничего не знают о том, каким компилятором собирали.
struct Diagnostic {
    std::string file;
    int line = 0;
    int col = 0;             // 0 — колонки не было (MSVC печатает её не всегда)
    std::string severity;    // "error" | "warning" | "note"; MSVC "fatal error" → "error"
    std::string code;        // "C2065" у MSVC; у clang/gcc пусто — там код едет внутри message
    std::string message;
};

// Извлекает диагностики из объединённого stdout+stderr компилятора. Строки без совпадения —
// игнорируются (заголовки, ^-указатели, линкер-шум).
std::vector<Diagnostic> parse_diagnostics(const std::string& compiler_output);

} // namespace ide::build
