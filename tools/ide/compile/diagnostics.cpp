#include "diagnostics.hpp"
#include "diagnostics_line.hpp"

#include <sstream>

namespace ide::build {

// Формат не выбирается по ОС и не задаётся флагом: на Windows живут оба (clang-cl печатает
// msvc-формат, а mingw-сборка того же дерева — gnu), и вывод одной сборки бывает смешанным,
// когда линкер и компилятор разные. Поэтому на каждой строке пробуются оба парсера.
//
// Порядок значим: gnu-парсер ищет ": error: " и на msvc-строке `a.cpp(1,2): error C2065: msg`
// не срабатывает (там между severity и двоеточием стоит код), а вот msvc-парсер на gnu-строке с
// путём вида `a(1).cpp:2:3: error: msg` сработал бы. Первым идёт более требовательный.
namespace {

// Срезает то, что печатает не компилятор: отступы и префикс проекта msbuild (`1>`, `12>`).
// Оставленный на месте, он уехал бы в d.file первым же substr — и click-to-open промахнулся бы
// по пути «1>C:\src\a.cpp», которого не существует.
std::string strip_prefix(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    size_t d = i;
    while (d < line.size() && line[d] >= '0' && line[d] <= '9') ++d;
    if (d > i && d < line.size() && line[d] == '>') i = d + 1;
    return line.substr(i);
}

} // namespace

std::vector<Diagnostic> parse_diagnostics(const std::string& compiler_output) {
    std::vector<Diagnostic> out;
    std::istringstream in(compiler_output);
    std::string raw;
    while (std::getline(in, raw)) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();   // CRLF из вывода cl.exe
        const std::string line = strip_prefix(raw);
        Diagnostic d;
        if (parse_gnu_line(line, d) || parse_msvc_line(line, d)) out.push_back(std::move(d));
    }
    return out;
}

} // namespace ide::build
