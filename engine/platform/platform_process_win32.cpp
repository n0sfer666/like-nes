#include "platform_process.hpp"

#include <windows.h>

#include "win32_spawn.hpp"

namespace platform {
namespace {

// Код, которым помечается насильственное завершение. Отличать «убит нами» от «успел выйти сам»
// на Windows больше нечем: сигналов нет, а обычный exit-код ребёнок выбирает сам.
constexpr DWORD KILL_CODE = 0x4B494C4Cu; // 'KILL'

void classify(DWORD code, ExitStatus& out) {
    out.code = static_cast<int>(code);
    if (code == KILL_CODE) {
        out.kind = ExitKind::Killed;
        return;
    }
    // Необработанное исключение доезжает кодом возврата, равным коду исключения: старшие два
    // бита NTSTATUS = 0b11 (severity ERROR) — так выглядят ACCESS_VIOLATION (0xC0000005),
    // ILLEGAL_INSTRUCTION, STACK_OVERFLOW. Обычный exit(N) туда не попадает: CRT возвращает
    // код из main, и уложиться в этот диапазон он мог бы только намеренно.
    out.kind = ((code & 0xC0000000u) == 0xC0000000u) ? ExitKind::Crashed : ExitKind::Exited;
}

} // namespace

uint32_t process_id() { return static_cast<uint32_t>(GetCurrentProcessId()); }

bool run_tool(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;
    PROCESS_INFORMATION pi{};
    if (!win32::spawn_process(argv, pi)) return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    const bool got = GetExitCodeProcess(pi.hProcess, &code) != 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return got && code == 0;
}

Child::~Child() { kill_and_wait(); }

Child::Child(Child&& o) noexcept : raw_(o.raw_) { o.raw_ = 0; }

Child& Child::operator=(Child&& o) noexcept {
    if (this != &o) {
        kill_and_wait();
        raw_ = o.raw_;
        o.raw_ = 0;
    }
    return *this;
}

bool Child::spawn(const std::vector<std::string>& argv) {
    if (argv.empty() || raw_ != 0) return false;
    PROCESS_INFORMATION pi{};
    if (!win32::spawn_process(argv, pi)) return false;
    CloseHandle(pi.hThread); // поток ребёнка нам не нужен, а хендл течёт до выхода процесса
    raw_ = reinterpret_cast<intptr_t>(pi.hProcess);
    return true;
}

bool Child::wait(ExitStatus& out) {
    out = ExitStatus{};   // не оставлять исход прошлого ребёнка в переиспользованной структуре
    if (raw_ == 0) return false;
    HANDLE h = reinterpret_cast<HANDLE>(raw_);
    raw_ = 0;
    // Конечное ожидание по той же причине, что и в kill_and_wait: упавший процесс на раннере
    // может неопределённо долго удерживаться WerFault, и INFINITE превратил бы упавший тест в
    // висящий до таймаута джоб. Не дождались — убиваем и возвращаем false (out остаётся Unknown).
    const DWORD w = WaitForSingleObject(h, 30000);
    DWORD code = 0;
    const bool got = (w == WAIT_OBJECT_0) && GetExitCodeProcess(h, &code) != 0;
    if (w != WAIT_OBJECT_0) TerminateProcess(h, KILL_CODE);
    CloseHandle(h);
    if (!got) return false;
    classify(code, out);
    return true;
}

bool run_capture(const std::vector<std::string>& argv, std::string& output, ExitStatus& status) {
    output.clear();
    status = ExitStatus{};
    if (argv.empty()) return false;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr;
    HANDLE wr = nullptr;
    if (CreatePipe(&rd, &wr, &sa, 0) == 0) return false;
    // Наследование ребёнку и так ограничено списком в spawn_process, но конец чтения снимается с
    // наследования ещё и здесь: список защищает только НАШИ запуски, а сторонний код в процессе
    // (плагин, чужая библиотека) вправе позвать CreateProcessW сам. Унаследовав конец чтения,
    // такой ребёнок держал бы канал открытым, и EOF ниже не наступил бы, пока он жив.
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi{};
    const bool started = win32::spawn_process(argv, pi, wr);
    CloseHandle(wr);   // своя копия конца записи закрывается СРАЗУ, иначе EOF не наступит никогда
    if (!started) {
        CloseHandle(rd);
        return false;
    }
    CloseHandle(pi.hThread);

    char buf[4096];
    DWORD n = 0;
    while (ReadFile(rd, buf, static_cast<DWORD>(sizeof(buf)), &n, nullptr) != 0 && n > 0)
        output.append(buf, n);
    CloseHandle(rd);

    // Вывод дочитан до EOF, то есть ребёнок уже закрыл свои концы — ждать здесь по сути нечего.
    // Конечный таймаут остаётся страховкой от единственного сценария, где это не так: упавший
    // процесс, которого удерживает WerFault. INFINITE превратил бы его в вечно висящий джоб CI.
    const DWORD w = WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 0;
    const bool got = (w == WAIT_OBJECT_0) && GetExitCodeProcess(pi.hProcess, &code) != 0;
    if (w != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, KILL_CODE);
    CloseHandle(pi.hProcess);
    if (!got) return false;
    classify(code, status);
    return true;
}

bool Child::kill_and_wait() {
    if (raw_ == 0) return false;
    HANDLE h = reinterpret_cast<HANDLE>(raw_);
    raw_ = 0;
    TerminateProcess(h, KILL_CODE); // уже мёртв → FALSE, это не ошибка, а исход «успел сам»
    // Конечное ожидание: TerminateProcess по процессу, ещё сидящему в загрузчике, — известный
    // источник зависаний, и INFINITE превратил бы такой случай в вечно висящий джоб CI вместо
    // упавшего теста. Тридцати секунд хватает любому здешнему ребёнку с запасом.
    if (WaitForSingleObject(h, 30000) != WAIT_OBJECT_0) {
        CloseHandle(h);
        return false;
    }
    DWORD code = 0;
    const bool got = GetExitCodeProcess(h, &code) != 0;
    CloseHandle(h);
    return got && code == KILL_CODE;
}

} // namespace platform
