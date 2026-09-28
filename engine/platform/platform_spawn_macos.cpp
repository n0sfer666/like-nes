#include "platform_spawn_posix.hpp"

#include <crt_externs.h>
#include <fcntl.h>
#include <unistd.h>

namespace platform {
namespace posix {

// CLOEXEC_DEFAULT закрывает в ребёнке всё, что не названо действиями над файлами, — включая
// stdin, поэтому 0 передаётся явно. Цикл close() до лимита здесь стоил бы 122 мс на запуск: у
// этой ОС мягкий лимит дескрипторов бывает 1048576.
//
// Закрытый stdin родителя (`<&-`, демон) addinherit_np не переживает: ядро отвечает EBADF, и
// отказывал бы КАЖДЫЙ запуск. Тогда 0 — /dev/null, как NUL у Windows-близнеца в win32_spawn.cpp.
bool inherit_only_std(posix_spawnattr_t& attr, posix_spawn_file_actions_t& fa) {
    const bool stdin_open = fcntl(STDIN_FILENO, F_GETFD) >= 0;
    const int in = stdin_open
                       ? posix_spawn_file_actions_addinherit_np(&fa, STDIN_FILENO)
                       : posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    return in == 0 && posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT) == 0;
}

// `environ` на macOS определён только в исполняемом файле, а platform_core линкуют и плагины.
char** current_environment() { return *_NSGetEnviron(); }

} // namespace posix
} // namespace platform
