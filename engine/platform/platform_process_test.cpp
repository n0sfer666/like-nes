#include "platform_args.hpp"
#include "platform_process.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

// Шов запуска процесса: захват вывода и классификация исхода. Обе реализации отдают «строку и
// код», и расхождение между ними молчит до живого прогона — потому тест гоняется на трёх ОС.
//
// Ребёнок здесь — сам тест, перезапущенный со служебным режимом (argv[1]): отдельная программа
// потребовала бы своей цели в CMake на каждой ОС, а fork'а на Windows нет. Путь к себе берётся из
// argv[0] — своей exe_path в шве пока нет, а запускают тест из CI и CMake всегда путём, не именем
// из PATH. Появится exe_path (шов #13 её не вводит) — заменить здесь.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Маркеры разные, чтобы «оба потока сведены в один» проверялось, а не подразумевалось: один и тот
// же текст в обоих потоках прошёл бы и при потере любого из них.
const char* OUT_MARK = "seam-stdout-marker";
const char* ERR_MARK = "seam-stderr-marker";
constexpr uint32_t BULK_LINES = 20000;

int child_mode(const std::string& mode) {
    if (mode == "both") {
        std::fputs(OUT_MARK, stdout);
        std::fflush(stdout);
        std::fputs(ERR_MARK, stderr);
        std::fflush(stderr);
        return 0;
    }
    if (mode == "bulk") {
        // Заведомо больше буфера канала (64 КБ у обеих ОС): на таком объёме ребёнок блокируется
        // на записи, пока родитель не вычитает, и вылезают частичные чтения — то, ради чего цикл
        // до EOF вообще написан. Компилятор с сотней ошибок печатает столько же.
        for (uint32_t i = 0; i < BULK_LINES; ++i) std::printf("line %u\n", i);
        std::fflush(stdout);
        std::fputs(ERR_MARK, stderr);
        std::fflush(stderr);
        return 0;
    }
    if (mode == "code") return 42;
    if (mode == "crash") {
        volatile int* p = nullptr;
        *p = 1;
        return 0;
    }
    return 3;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc >= 2) return child_mode(argv[1]);

    const std::string self = argv[0];
    std::string out;
    platform::ExitStatus st;

    check(platform::run_capture({self, "both"}, out, st), "capturing a successful run reports success");
    check(out.find(OUT_MARK) != std::string::npos, "stdout of the child is captured");
    check(out.find(ERR_MARK) != std::string::npos, "stderr of the child is captured too");
    check(st.kind == platform::ExitKind::Exited && st.code == 0, "clean exit is reported as Exited(0)");

    check(platform::run_capture({self, "bulk"}, out, st), "a chatty child is reaped, not deadlocked");
    check(st.kind == platform::ExitKind::Exited && st.code == 0, "the chatty child exits cleanly");
    // Последняя строка И stderr-маркер: потеря хвоста — самый вероятный дефект цикла чтения, а
    // ассерт «вывод непустой» на ней бы не сработал.
    check(out.find("line " + std::to_string(BULK_LINES - 1) + "\n") != std::string::npos,
          "the last line of a large output survives");
    check(out.find(ERR_MARK) != std::string::npos, "stderr survives a large stdout too");

    check(platform::run_capture({self, "code"}, out, st), "capturing a failing run still reports success");
    check(st.kind == platform::ExitKind::Exited && st.code == 42, "the child's own exit code survives");
    check(out.empty(), "a silent child yields empty output, not stale text");

    // Падение компилятора обязано отличаться от «собралось с ошибками»: обе реализации сводят
    // нарушение доступа к одному ExitKind, хотя приезжает оно сигналом против кода исключения.
    check(platform::run_capture({self, "crash"}, out, st), "a crashing child is still reaped");
    if (st.kind != platform::ExitKind::Crashed)
        std::printf("  (exit kind=%d code=%d)\n", static_cast<int>(st.kind), st.code);
    check(st.kind == platform::ExitKind::Crashed, "a crash is not reported as a clean exit");

    out = "untouched";
    check(!platform::run_capture({}, out, st), "an empty argv is refused");
    check(out.empty(), "a refused run clears the output buffer");
    check(st.kind == platform::ExitKind::Unknown, "a refused run leaves no stale exit status");
    check(!platform::run_capture({self + "_does_not_exist_hopefully"}, out, st) ||
              st.code != 0,
          "a missing program never looks like a successful build");

    const bool pass = (fails == 0);
    std::printf("platform-process: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
