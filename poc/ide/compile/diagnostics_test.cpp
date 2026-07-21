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

    // краевые: пустой вход, битые префиксы
    check(parse_diagnostics("").empty(), "empty input -> no diagnostics");
    check(parse_diagnostics("error: no location prefix\n").empty(), "no file:line -> ignored");
    check(parse_diagnostics("file.cpp: error: missing line/col\n").empty(), "missing line/col -> ignored");

    bool pass = (failures == 0);
    std::printf("ide-diagnostics: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
