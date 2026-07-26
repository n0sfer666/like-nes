#pragma once
#include <string>
#include <vector>

// Парсер диагностик компилятора (спека #7, гейт 5). clang/gcc-формат:
//   path/file.cpp:LINE:COL: severity: message
// → структурные записи для build-панели редактора + click-to-open на file:line.
namespace ide::build {

struct Diagnostic {
    std::string file;
    int line = 0;
    int col = 0;
    std::string severity;   // "error" | "warning" | "note"
    std::string message;
};

// Извлекает диагностики из объединённого stdout+stderr компилятора. Строки без совпадения —
// игнорируются (заголовки, ^-указатели, линкер-шум).
std::vector<Diagnostic> parse_diagnostics(const std::string& compiler_output);

} // namespace ide::build
