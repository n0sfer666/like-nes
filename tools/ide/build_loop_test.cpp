#include "compile/build_orchestrator.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "platform_module.hpp"
#include "platform_process.hpp"
#include "platform_watch.hpp"

// Гейт 5 (спека #7): watch .cpp → build (реальный компилятор) → hot-reload модуля + панель ошибок.
// Host-owned состояние (acc) переживает reload; правка source → смена поведения; битый source →
// build fail + диагностики с file:line (click-to-open).
//
// POSIX-вызовов здесь больше нет (спека #13): dlopen → platform::Module, mkdtemp → каталог рядом
// с exe, поллинг mtime → platform::Watcher. Всё, что различается по компилятору — командная
// строка сборки и префикс экспорта, — приходит СТРОКОЙ из CMake (IDE_BUILD_ARGV/IDE_EXPORT), а не
// условной компиляцией: тест не должен знать, на какой он ОС.
using namespace ide::build;

namespace {

int failures = 0;
void check(bool c, const char* w) {
    if (!c) {
        std::printf("  FAIL: %s\n", w);
        ++failures;
    }
}

void write_file(const std::string& path, const std::string& content) {
    std::FILE* f = platform::open_file(path, "wb");
    if (!f) return;
    std::fwrite(content.data(), 1, content.size(), f);
    platform::sync_file(f);
    std::fclose(f);
}

// Шаблон командной строки из CMake: токены через '|', плейсхолдеры {src}/{obj}/{out}.
// Разделитель не ';' — в CMake это разделитель списка, и define разъехался бы на семь.
std::vector<std::string> build_argv(const std::string& src, const std::string& obj,
                                    const std::string& out) {
    std::vector<std::string> argv;
    const std::string tmpl = IDE_BUILD_ARGV;
    for (size_t pos = 0; pos <= tmpl.size();) {
        const size_t sep = tmpl.find('|', pos);
        std::string tok = tmpl.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
        for (const auto& [key, value] : {std::pair<std::string, std::string>{"{src}", src},
                                         {"{obj}", obj},
                                         {"{out}", out}}) {
            const size_t at = tok.find(key);
            if (at != std::string::npos) tok.replace(at, key.size(), value);
        }
        if (!tok.empty()) argv.push_back(tok);
        if (sep == std::string::npos) break;
        pos = sep + 1;
    }
    return argv;
}

// Правка исходника глазами watcher'а: пишем и ждём событие именно на нём. Один poll не годится —
// бэкенды режут пачку по-разному, и сборочные артефакты в том же каталоге приезжают вперемешку.
bool edit_and_observe(platform::Watcher& w, const std::string& src, const std::string& body) {
    write_file(src, body);
    std::vector<std::string> changed;
    for (int i = 0; i < 10; ++i) {
        if (!w.poll(changed, 500)) return false;
        for (const std::string& p : changed)
            if (p.size() >= 12 && p.compare(p.size() - 12, 12, "gameplay.cpp") == 0) return true;
    }
    return false;
}

std::string source(const char* expr) {
    return std::string("extern \"C\" ") + IDE_EXPORT + "int step(int x){ return " + expr + "; }\n";
}

using step_fn = int (*)(int);

} // namespace

int main() {
    const std::string dir =
        platform::exe_dir() + "/likenes_bl_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(dir)) {
        std::printf("cannot create %s\n", dir.c_str());
        return 3;
    }
    const std::string src = dir + "/gameplay.cpp";
    const std::string obj = dir + "/gameplay.obj";
    const std::string out = dir + "/gameplay" + IDE_MODULE_SUFFIX;

    // Каталог сборки — рабочий: MSVC кладёт промежуточные файлы рядом с cwd, и мусорить
    // каталогом бинарников незачем.
    platform::Watcher watcher;
    check(watcher.watch_dir(dir, /*recursive=*/false), "watcher started on source dir");

    // 1) v1: step(x)=x+1 → правку видит watcher, сборка проходит без диагностик.
    check(edit_and_observe(watcher, src, source("x + 1")), "watcher observes v1 source");
    BuildResult b1 = run_build(build_argv(src, obj, out));
    check(b1.success, "build v1 succeeds");
    check(b1.diagnostics.empty(), "build v1 has no diagnostics");

    // 2) load v1, host-owned acc, 3 тика → +3.
    int acc = 0;
    {
        platform::Module m;
        check(m.open(out), "load v1 module");
        auto f1 = reinterpret_cast<step_fn>(m.symbol("step"));
        check(f1 != nullptr, "load v1 step symbol");
        for (int i = 0; i < 3 && f1; ++i) acc = f1(acc);
    }
    check(acc == 3, "v1 behavior: acc==3");

    // 3) «разработчик отредактировал»: v2 step(x)=x+10 → watcher → rebuild → hot-reload.
    check(edit_and_observe(watcher, src, source("x + 10")), "watcher observes the edit");
    BuildResult b2 = run_build(build_argv(src, obj, out));
    check(b2.success, "rebuild v2 succeeds");
    {
        platform::Module m;
        check(m.open(out), "hot-reload v2 module");
        auto f2 = reinterpret_cast<step_fn>(m.symbol("step"));
        check(f2 != nullptr, "hot-reload v2 step symbol");
        for (int i = 0; i < 3 && f2; ++i) acc = f2(acc);   // acc пережил reload (host-owned)
    }
    check(acc == 33, "state survived reload + behavior changed: acc==33 (3 + 3*10)");

    // 4) битая правка → build fail + диагностики с file:line (click-to-open в панель).
    check(edit_and_observe(watcher, src, source("x + undefined_symbol_here")),
          "watcher observes the broken edit");
    BuildResult b3 = run_build(build_argv(src, obj, out));
    check(!b3.success, "build with error fails");
    check(!b3.diagnostics.empty(), "error surfaced as diagnostic(s)");
    bool has_error = false;
    for (const auto& d : b3.diagnostics)
        if (d.severity == "error" && d.line > 0 && !d.file.empty()) {
            has_error = true;
            break;
        }
    check(has_error, "diagnostic has severity=error + file:line (click-to-open)");

    // Наблюдение снимается ДО уборки: на Windows живой хендл каталога не даёт его удалить.
    watcher.close();
    platform::remove_file(src);
    platform::remove_file(obj);
    platform::remove_file(out);

    const bool pass = failures == 0;
    std::printf("ide-build-loop: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
