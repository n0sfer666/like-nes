#pragma once
#include "diagnostics.hpp"
#include <cstdint>
#include <string>
#include <vector>

// Оркестрация сборки (спека #7, гейт 5): watch .cpp (mtime) → инкрементальный build (реальный
// компилятор, захват вывода) → диагностики в панель. Успех → триггер hot-reload .so (host).
namespace ide::build {

struct BuildResult {
    bool success = false;
    int exit_code = -1;
    std::vector<Diagnostic> diagnostics;   // → build-панель редактора
    std::string raw_output;
};

// Запускает argv (напр. {"c++","-shared","-fPIC","src.cpp","-o","out.so"}), захватывает
// объединённый stdout+stderr, парсит диагностики. POSIX (fork/exec/pipe).
BuildResult run_build(const std::vector<std::string>& argv);

} // namespace ide::build
