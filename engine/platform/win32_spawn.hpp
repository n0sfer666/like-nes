#pragma once
#include <windows.h>

#include <string>
#include <vector>

// Запуск процесса через CreateProcessW. Собирается ТОЛЬКО на Windows.
//
// Отдельная единица, а не приватная функция process-шва, по той же причине, что и сборка
// командной строки: сюда переехало ограничение наследования хендлов (STARTUPINFOEX +
// PROC_THREAD_ATTRIBUTE_HANDLE_LIST) — механика на полсотни строк, у которой одна
// ответственность и ни одного отношения к «дождаться и классифицировать исход».
namespace platform::win32 {

// out — куда сводить stdout+stderr ребёнка; INVALID_HANDLE_VALUE означает «в NUL».
// Ребёнок получает РОВНО три стандартных хендла и ни одного больше: у Windows нет CLOEXEC, а
// bInheritHandles=TRUE без списка отдал бы ребёнку каждый наследуемый хендл процесса. Для канала
// это не гигиена, а живучесть: унаследованный чужим ребёнком конец записи не даёт EOF тому, кто
// читает наш, — и read виснет навсегда.
bool spawn_process(const std::vector<std::string>& argv, PROCESS_INFORMATION& pi,
                   HANDLE out = INVALID_HANDLE_VALUE);

} // namespace platform::win32
