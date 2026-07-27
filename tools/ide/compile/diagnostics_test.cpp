#include "diagnostics.hpp"
#include <cstdio>
#include <string>

// Детерм. unit парсера диагностик (гейт 5, click-to-open). Фикс. вход → cross-OS, без зависимости
// от реального вывода компилятора (формат clang/gcc стабилен).
using namespace ide::build;

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }
} // namespace

int main() {
    const std::string out =
        "gameplay.cpp:1:40: error: use of undeclared identifier 'foo'\n"
        "    return x + foo;\n"
        "                 ^\n"
        "src/dir/gameplay.cpp:2:5: warning: unused variable 'y' [-Wunused-variable]\n"
        "gameplay.cpp:1:40: note: expanded from here\n"
        "ld: symbol(s) not found for architecture arm64\n"
        "clang: error: linker command failed\n";   // без file:line:col → игнор

    auto d = parse_diagnostics(out);
    check(d.size() == 3, "parses exactly 3 file:line diagnostics (linker-noise ignored)");
    if (d.size() == 3) {
        check(d[0].file == "gameplay.cpp" && d[0].line == 1 && d[0].col == 40, "d0 file:line:col");
        check(d[0].severity == "error", "d0 severity=error");
        check(d[0].message == "use of undeclared identifier 'foo'", "d0 message");
        check(d[1].file == "src/dir/gameplay.cpp" && d[1].line == 2, "d1 path with slashes + line");
        check(d[1].severity == "warning", "d1 severity=warning");
        check(d[2].severity == "note" && d[2].line == 1 && d[2].col == 40, "d2 note file:line:col");
    }

    // --- Формат MSVC/clang-cl (решение 3 спеки #13) ---
    // Взято с реального вывода cl.exe: путь с буквой диска и обратными слэшами, колонка есть не
    // всегда, «fatal error» и линкерная строка без локации.
    const std::string msvc =
        "C:\\src\\gameplay.cpp(12,5): error C2065: 'foo': undeclared identifier\n"
        "C:\\src\\gameplay.cpp(3): warning C4101: 'y': unreferenced local variable\n"
        "src\\gameplay.cpp(1,1): fatal error C1004: unexpected end-of-file found\n"
        "gameplay.cpp(9,2): note: see declaration of 'bar'\n"
        "LINK : fatal error LNK1104: cannot open file 'x.obj'\n";   // без локации → игнор

    auto m = parse_diagnostics(msvc);
    check(m.size() == 4, "msvc: parses 4 located diagnostics, linker line ignored");
    if (m.size() == 4) {
        check(m[0].file == "C:\\src\\gameplay.cpp" && m[0].line == 12 && m[0].col == 5,
              "msvc: drive letter in the path does not break the location");
        check(m[0].severity == "error" && m[0].code == "C2065", "msvc: severity and code split off");
        check(m[0].message == "'foo': undeclared identifier", "msvc: the code stays out of message");
        check(m[1].line == 3 && m[1].col == 0, "msvc: a diagnostic without a column keeps col=0");
        check(m[2].severity == "error" && m[2].code == "C1004", "msvc: fatal error normalizes to error");
        check(m[3].severity == "note" && m[3].code.empty() &&
                  m[3].message == "see declaration of 'bar'",
              "msvc: clang-cl note without a code");
    }
    // msbuild печатает номер проекта перед строкой компилятора, а вложенные проекты — двузначный.
    auto pfx = parse_diagnostics("1>C:\\src\\a.cpp(2,3): error C2065: x\n"
                                 "   12>b.cpp(4): warning C4101: y\n");
    check(pfx.size() == 2, "msbuild project prefixes do not hide diagnostics");
    if (pfx.size() == 2) {
        check(pfx[0].file == "C:\\src\\a.cpp", "the '1>' prefix stays out of the file path");
        check(pfx[1].file == "b.cpp", "indentation and a two-digit prefix are stripped too");
    }
    // Пути с пробелами и скобками — штатный вид системных заголовков MSVC. Скобка локации всегда
    // последняя, но пиннится это тестом, а не устройством поиска.
    auto sp = parse_diagnostics(
        "C:\\Program Files (x86)\\MSVC\\include\\xstring(1234,5): warning C4244: narrowing\n");
    check(sp.size() == 1 && sp[0].line == 1234, "parentheses inside the path do not eat the location");
    if (sp.size() == 1)
        check(sp[0].file == "C:\\Program Files (x86)\\MSVC\\include\\xstring", "the whole path survives");

    // Двоеточие в тексте ошибки — не разделитель кода: иначе половина сообщения уезжает в code.
    auto nc = parse_diagnostics("a.cpp(23): error while processing module: bad file\n");
    check(nc.empty(), "a prose colon is not mistaken for a diagnostic code");
    check(parse_diagnostics("a.cpp(1): error\n").empty(), "a truncated line yields nothing, not garbage");
    // Локализованный cl.exe (VSLANG не выставлен) — известная слепая зона: русский severity парсер
    // не знает и знать не должен, потому run_build форсирует VSLANG=1033.
    check(parse_diagnostics("a.cpp(1,2): ошибка C2065: foo\n").empty(),
          "a localized severity is not guessed at");

    // Один вывод бывает смешанным (компилятор clang-cl, линкер MSVC) — парсер не переключается
    // на формат целиком, а решает построчно.
    check(parse_diagnostics("a.cpp:1:2: error: gnu\nb.cpp(3,4): error C1: msvc\n").size() == 2,
          "both formats parse out of a single mixed output");
    check(parse_diagnostics("cl : Command line warning D9025 : overriding '/W4' with '/w'\n").empty(),
          "a driver warning without a location is not a diagnostic");

    // краевые: пустой вход, битые префиксы
    check(parse_diagnostics("").empty(), "empty input -> no diagnostics");
    check(parse_diagnostics("error: no location prefix\n").empty(), "no file:line -> ignored");
    check(parse_diagnostics("file.cpp: error: missing line/col\n").empty(), "missing line/col -> ignored");

    bool pass = (failures == 0);
    std::printf("ide-diagnostics: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
