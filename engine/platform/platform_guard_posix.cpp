#include "platform_guard.hpp"

#include <csetjmp>
#include <csignal>
#include <cstring>

namespace platform {
namespace {

// Кадр возврата — на поток, а не на процесс: обработчик сигнала выполняется в том потоке, где
// упало, и общий буфер увёл бы прыжок в чужой (возможно, уже разрушенный) стек. Установка
// обработчиков, наоборот, процессная — sigaction иначе не бывает.
thread_local sigjmp_buf g_jmp;
thread_local volatile sig_atomic_t g_armed = 0;
volatile sig_atomic_t g_installed = 0;

void crash_handler(int sig) {
    // Не взведён — значит упало не в плагине: восстанавливаем поведение по умолчанию и
    // добиваем процесс тем же сигналом, чтобы падение движка осталось падением движка.
    if (!g_armed) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
        return;
    }
    g_armed = 0;
    siglongjmp(g_jmp, sig);
}

} // namespace

void install_crash_isolation() {
    if (g_installed) return;
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    // Набор — двойник фильтра SEH: SIGSEGV/SIGBUS ↔ ACCESS_VIOLATION/IN_PAGE_ERROR/
    // DATATYPE_MISALIGNMENT, SIGILL ↔ ILLEGAL_INSTRUCTION/PRIV_INSTRUCTION. Без SIGILL плагин,
    // собранный под чужой набор инструкций, ронял бы редактор на POSIX и не ронял на Windows —
    // то есть инвариант 4 архитектуры выполнялся бы через раз.
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    g_installed = 1;
}

bool guarded_call(GuardedFn fn, void* arg) {
    install_crash_isolation();
    // Вложенный вызов своего кадра НЕ ставит: перезапись g_jmp увела бы последующий прыжок в
    // уже разрушенный фрейм, а снятие g_armed на выходе внутреннего вызова разоружило бы
    // внешний. Падение внутри уходит во внешний кадр — он и вернёт false (см. platform_guard.hpp).
    if (g_armed) {
        fn(arg);
        return true;
    }
    if (sigsetjmp(g_jmp, 1) == 0) {
        g_armed = 1;
        fn(arg);
        g_armed = 0;
        return true;
    }
    g_armed = 0;
    return false;
}

} // namespace platform
