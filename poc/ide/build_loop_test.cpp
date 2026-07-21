#include "compile/build_orchestrator.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unistd.h>

// Гейт 5 (спека #7): watch .cpp → build (реальный компилятор) → hot-reload .so + панель ошибок.
// Host-owned состояние (acc) переживает reload; правка source → смена поведения; битый source →
// build fail + диагностики с file:line (click-to-open). argv[1] = компилятор (по умолч. c++).
using namespace ide::build;

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }

void write_file(const std::string& path, const std::string& content) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f) { std::fwrite(content.data(), 1, content.size(), f); std::fclose(f); }
}

typedef int (*step_fn)(int);

// dlopen .so, вернуть step; nullptr при ошибке.
step_fn load_step(const std::string& so, void** handle) {
    *handle = dlopen(so.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!*handle) return nullptr;
    return reinterpret_cast<step_fn>(dlsym(*handle, "step"));
}
} // namespace

#ifndef IDE_CXX
#define IDE_CXX "c++"
#endif

int main(int argc, char** argv) {
    const std::string cc = (argc > 1) ? argv[1] : IDE_CXX;

    char tmpl[] = "/tmp/likenes_bl_XXXXXX";
    char* dir = mkdtemp(tmpl);
    if (!dir) { std::printf("mkdtemp fail\n"); return 3; }
    const std::string src = std::string(dir) + "/gameplay.cpp";
    const std::string so = std::string(dir) + "/gameplay.so";

    auto build = [&]() {
        return run_build({cc, "-shared", "-fPIC", "-O2", src, "-o", so});
    };

    int64_t mtime = 0;

    // 1) v1: step(x)=x+1 → build ok, без диагностик.
    write_file(src, "extern \"C\" int step(int x){ return x + 1; }\n");
    file_changed(src, mtime);
    BuildResult b1 = build();
    check(b1.success, "build v1 succeeds");
    check(b1.diagnostics.empty(), "build v1 has no diagnostics");

    // 2) load v1, host-owned acc, 3 тика → +3.
    void* h1 = nullptr;
    step_fn f1 = load_step(so, &h1);
    check(f1 != nullptr, "load v1 step symbol");
    int acc = 0;
    for (int i = 0; i < 3 && f1; ++i) acc = f1(acc);
    check(acc == 3, "v1 behavior: acc==3");
    if (h1) dlclose(h1);

    // 3) "разработчик отредактировал": v2 step(x)=x+10. watch детектит, rebuild, hot-reload.
    write_file(src, "extern \"C\" int step(int x){ return x + 10; }\n");
    bool changed = file_changed(src, mtime);
    check(changed, "watcher detects source edit (mtime)");
    BuildResult b2 = build();
    check(b2.success, "rebuild v2 succeeds");
    void* h2 = nullptr;
    step_fn f2 = load_step(so, &h2);
    check(f2 != nullptr, "hot-reload v2 step symbol");
    for (int i = 0; i < 3 && f2; ++i) acc = f2(acc);   // acc пережил reload (host-owned)
    check(acc == 33, "state survived reload + behavior changed: acc==33 (3 + 3*10)");
    if (h2) dlclose(h2);

    // 4) битая правка → build fail + диагностики с file:line (click-to-open в панель).
    write_file(src, "extern \"C\" int step(int x){ return x + undefined_symbol_here; }\n");
    file_changed(src, mtime);
    BuildResult b3 = build();
    check(!b3.success, "build with error fails");
    check(!b3.diagnostics.empty(), "error surfaced as diagnostic(s)");
    bool has_error = false;
    for (const auto& d : b3.diagnostics)
        if (d.severity == "error" && d.line > 0 && !d.file.empty()) { has_error = true; break; }
    check(has_error, "diagnostic has severity=error + file:line (click-to-open)");

    // cleanup
    unlink(src.c_str());
    unlink(so.c_str());
    rmdir(dir);

    bool pass = (failures == 0);
    std::printf("ide-build-loop: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
