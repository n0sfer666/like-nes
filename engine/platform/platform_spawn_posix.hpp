#pragma once
#include <spawn.h>

// Часть запуска процесса, в которой POSIX не общий: как сказать posix_spawn «ребёнку — только
// 0/1/2». На macOS это флаг атрибутов, на Linux — действие над файлами, и оба не переносимы.
// Реализации — platform_spawn_macos.cpp и platform_spawn_linux.cpp, выбирает CMake.
namespace platform {
namespace posix {

// Зовётся ПОСЛЕДНИМ действием над fa: всё, что действия выше положили в 1 и 2, выживает, прочее
// закрывается. Windows-близнец держит то же свойство списком хендлов в win32_spawn.cpp.
bool inherit_only_std(posix_spawnattr_t& attr, posix_spawn_file_actions_t& fa);

char** current_environment();

} // namespace posix
} // namespace platform
