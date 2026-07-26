#include "platform_guard.hpp"

#include <windows.h>

namespace platform {
namespace {

// Набор — двойник SIGSEGV/SIGBUS/SIGILL у POSIX-реализации. Всё прочее уходит наружу:
// переполнение стека сюда намеренно НЕ входит — после него гарантий на продолжение нет,
// страница-сторож уже израсходована, и «пойманное» падение обернулось бы падением на следующем
// кадре.
LONG filter(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
        return EXCEPTION_EXECUTE_HANDLER;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

} // namespace

void install_crash_isolation() {}

// Ни одного объекта с деструктором в теле: функция с __except не умеет раскручивать стек
// (MSVC C2712), и любая локальная RAII-переменная тут — ошибка компиляции, а не стиль.
bool guarded_call(GuardedFn fn, void* arg) {
    __try {
        fn(arg);
        return true;
    } __except (filter(GetExceptionCode())) {
        return false;
    }
}

} // namespace platform
