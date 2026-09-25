#include "platform_args.hpp"
#include "platform_net.hpp"
#include "platform_process.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

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

constexpr size_t EXACT_BYTES = 100000;
constexpr size_t FLOOD_CAP = size_t{1} << 20;

// Пишет n байт и выходит; n == 0 — пишет, пока не откажет запись или не наберёт 256 МиБ. Верхняя
// граница нужна прежней реализации без потолка: она дочитала бы поток и упала на утверждении, а
// не повисла до таймаута джоба.
int write_bytes(size_t n) {
    static char block[65536];
    std::memset(block, 'x', sizeof(block));
    const size_t total = n != 0 ? n : size_t{256} << 20;
    for (size_t done = 0; done < total;) {
        const size_t k = total - done < sizeof(block) ? total - done : sizeof(block);
        if (std::fwrite(block, 1, k, stdout) != k) return 0;
        done += k;
    }
    std::fflush(stdout);
    return 0;
}

int child_mode(const std::string& mode, const std::string& self) {
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
    if (mode == "exact") return write_bytes(EXACT_BYTES);
    if (mode == "flood") return write_bytes(0);
    if (mode == "code") return 42;
    // Закрытый stdin родителя (`<&-`, демон) не должен ронять запуск: внук обязан стартовать и
    // вернуть свой код. std::fclose, а не close(0), — тот же режим гоняется и на Windows.
    if (mode == "nostdin") {
        std::fclose(stdin);
        std::string o;
        platform::ExitStatus s;
        const bool ran = platform::run_capture({self, "code"}, o, s);
        return ran && s.kind == platform::ExitKind::Exited && s.code == 42 ? 0 : 1;
    }
    if (mode == "hold") {
        // Живёт, пока родитель не убьёт; потолок — чтобы осиротевший ребёнок не висел вечно.
        for (int i = 0; i < 600; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 0;
    }
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
    if (argc >= 2) return child_mode(argv[1], argv[0]);

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
    // ассерт «вывод непустой» на ней бы не сработал. Перевод строки в искомое НЕ входит: stdout
    // ребёнка текстовый, и на Windows та же `printf` кладёт в канал CRLF — шов отдаёт байты как
    // есть, а нормализацией занимается разбор диагностик. Номер последней строки уникален, так
    // что как признак хвоста подстроки без терминатора достаточно.
    check(out.find("line " + std::to_string(BULK_LINES - 1)) != std::string::npos,
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

    // Потолок захвата (аудит #21, A·1·3) — на обоих концах: ровно потолок до EOF ещё не усечение,
    // байт сверх него — уже усечение, и output держит ровно потолок, а не «сколько успело прийти».
    check(platform::run_capture({self, "exact"}, out, st, EXACT_BYTES), "output of exactly the cap is reaped");
    check(st.kind == platform::ExitKind::Exited && out.size() == EXACT_BYTES,
          "output of exactly the cap is not truncated");
    check(platform::run_capture({self, "exact"}, out, st, EXACT_BYTES - 1), "output over the cap is reaped");
    if (st.kind != platform::ExitKind::Truncated)
        std::printf("  (exit kind=%d, %zu bytes)\n", static_cast<int>(st.kind), out.size());
    check(st.kind == platform::ExitKind::Truncated && out.size() == EXACT_BYTES - 1,
          "one byte over the cap is truncated to the cap");
    check(platform::run_capture({self, "flood"}, out, st, FLOOD_CAP), "an endless writer is reaped");
    check(st.kind == platform::ExitKind::Truncated && out.size() == FLOOD_CAP,
          "an endless writer is cut at the cap, not read to exhaustion");

    // Ребёнок получает только 0/1/2 (аудит #21, A·1·1). Судится ИСХОД, а не номер дескриптора:
    // унаследуй ребёнок привязанный сокет, порт остался бы занят после закрытия его в родителе.
    // Сокет открыт ДО запуска — именно так его получает сетевой тест с пирами-детьми.
    {
        namespace net = platform::net;
        net::Socket taken;
        check(taken.open(net::ADDRESS_LOOPBACK, 0), "a loopback socket for the inheritance probe opens");
        const uint16_t port = taken.local().port;
        platform::Child holder;
        check(holder.spawn({self, "hold"}), "a long-lived child spawns");
        taken.close();
        net::Socket again;
        check(again.open(net::ADDRESS_LOOPBACK, port),
              "a port closed by the parent is free while the child lives (no inherited socket)");
        check(holder.kill_and_wait(), "the long-lived child is killed");
    }

    out = "untouched";
    check(!platform::run_capture({}, out, st), "an empty argv is refused");
    check(out.empty(), "a refused run clears the output buffer");
    check(st.kind == platform::ExitKind::Unknown, "a refused run leaves no stale exit status");
    // Отсутствующая программа — отказ запуска, а не Exited(127): так на трёх ОС с перехода POSIX
    // на posix_spawnp, и CreateProcessW отказывает так же.
    const std::string missing = self + "_does_not_exist_hopefully";
    check(!platform::run_capture({missing}, out, st), "a missing program is refused by run_capture");
    platform::Child ghost;
    check(!ghost.spawn({missing}), "a missing program is refused by Child::spawn");

    check(platform::run_capture({self, "nostdin"}, out, st) &&
              st.kind == platform::ExitKind::Exited && st.code == 0,
          "a parent with a closed stdin still spawns children");

    const bool pass = (fails == 0);
    std::printf("platform-process: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
