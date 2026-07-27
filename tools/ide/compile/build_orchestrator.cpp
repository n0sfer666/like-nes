#include "build_orchestrator.hpp"
#include <vector>

#include "platform_fs.hpp"
#include "platform_process.hpp"

namespace ide::build {

// Запуск компилятора с захватом вывода — целиком в шве (platform::run_capture): здесь остаётся
// только разбор. Собственных fork/pipe у оркестратора больше нет, поэтому цель собирается на трёх ОС.
BuildResult run_build(const std::vector<std::string>& argv) {
    BuildResult r;
    if (argv.empty()) return r;

    platform::ExitStatus st;
    const bool reaped = platform::run_capture(argv, r.raw_output, st);
    // Разбор идёт ВСЕГДА: вывод, успевший приехать до падения или таймаута, — это диагностики, а
    // пустая панель при провалившейся сборке читается как «ошибок нет».
    // Упавший компилятор при этом не «сборка с ошибками»: exit_code остаётся -1, success ложен.
    r.exit_code = (reaped && st.kind == platform::ExitKind::Exited) ? st.code : -1;
    r.success = (r.exit_code == 0);
    r.diagnostics = parse_diagnostics(r.raw_output);
    return r;
}

bool file_changed(const std::string& path, int64_t& last_token) {
    int64_t token = 0;
    if (!platform::file_stamp(path, token)) return false;   // отсутствие/удаление ≠ изменение
    if (token == last_token) return false;
    last_token = token;
    return true;
}

} // namespace ide::build
