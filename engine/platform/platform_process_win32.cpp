#include "platform_process.hpp"

#include <windows.h>

#include "win32_cmdline.hpp"
#include "win32_utf.hpp"

namespace platform {
namespace {

// Код, которым помечается насильственное завершение. Отличать «убит нами» от «успел выйти сам»
// на Windows больше нечем: сигналов нет, а обычный exit-код ребёнок выбирает сам.
constexpr DWORD KILL_CODE = 0x4B494C4Cu; // 'KILL'

bool create(const std::vector<std::string>& argv, PROCESS_INFORMATION& pi) {
    std::vector<std::wstring> wargv;
    wargv.reserve(argv.size());
    for (const std::string& a : argv) {
        std::wstring wide = win32::widen(a);
        // Пустой аргумент легален (уедет как ""), пустой результат конвертации — нет.
        if (wide.empty() && !a.empty()) return false;
        wargv.push_back(std::move(wide));
    }
    std::wstring cmdline = win32::command_line(wargv);

    // NUL вместо /dev/null; хендл наследуемый, иначе ребёнок его не увидит.
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE null_out = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (null_out == INVALID_HANDLE_VALUE) return false;

    // STARTF_USESTDHANDLES требует все три хендла валидными и наследуемыми — иначе CreateProcessW
    // отказывает целиком. Собственный stdin ни того, ни другого не гарантирует: у процесса без
    // консоли (служба, GUI-хост редактора) GetStdHandle отдаёт NULL, а унаследованный хендл
    // бывает ненаследуемым. Подставляем NUL — ни один здешний ребёнок stdin не читает.
    DWORD in_flags = 0;
    HANDLE std_in = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE null_in = INVALID_HANDLE_VALUE;
    if (std_in == nullptr || std_in == INVALID_HANDLE_VALUE ||
        GetHandleInformation(std_in, &in_flags) == 0 || (in_flags & HANDLE_FLAG_INHERIT) == 0) {
        null_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (null_in == INVALID_HANDLE_VALUE) {
            CloseHandle(null_out);
            return false;
        }
        std_in = null_in;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = std_in;
    si.hStdOutput = null_out;
    si.hStdError = null_out;

    // lpApplicationName = nullptr: имя программы берётся из строки, а значит работает поиск по
    // PATH и автодобавление .exe — ровно как execvp у POSIX-близнеца.
    // inherit=TRUE обязателен для трёх хендлов выше и наследует все наследуемые хендлы процесса;
    // список PROC_THREAD_ATTRIBUTE_HANDLE_LIST не заводим — движок сам наследуемых хендлов не
    // создаёт, а лишний STARTUPINFOEX здесь стоил бы дороже, чем закрывает.
    const BOOL ok = CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, /*inherit=*/TRUE, 0,
                                   nullptr, nullptr, &si, &pi);
    if (null_in != INVALID_HANDLE_VALUE) CloseHandle(null_in);
    CloseHandle(null_out);
    return ok != 0;
}

} // namespace

bool run_tool(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;
    PROCESS_INFORMATION pi{};
    if (!create(argv, pi)) return false;

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
    if (!create(argv, pi)) return false;
    CloseHandle(pi.hThread); // поток ребёнка нам не нужен, а хендл течёт до выхода процесса
    raw_ = reinterpret_cast<intptr_t>(pi.hProcess);
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
