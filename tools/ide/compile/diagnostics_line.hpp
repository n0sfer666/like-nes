#pragma once
#include "diagnostics.hpp"

#include <string>

// Построчные парсеры под конкретный формат компилятора. Внутренний заголовок: наружу торчит одна
// `parse_diagnostics`, потому что панели редактора всё равно, чем собирали (решение 3 спеки #13).
//
// Каждый формат — своя единица трансляции, а не ветка в общей функции: у них нет ни одной общей
// строчки разбора, кроме «нашли — заполнили Diagnostic», а сцепить их в один if значило бы
// подпирать чужой формат чужими эвристиками при первой же правке.
namespace ide::build {

// path/file.cpp:LINE:COL: severity: message — clang, gcc.
bool parse_gnu_line(const std::string& line, Diagnostic& out);

// path\file.cpp(LINE,COL): severity CODE: message — MSVC, clang-cl.
bool parse_msvc_line(const std::string& line, Diagnostic& out);

} // namespace ide::build
