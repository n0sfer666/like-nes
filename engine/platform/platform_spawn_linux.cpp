#include "platform_spawn_posix.hpp"

#include <unistd.h>

namespace platform {
namespace posix {

// glibc 2.34+: закрытие делает close_range ядра, а не цикл до лимита дескрипторов.
bool inherit_only_std(posix_spawnattr_t&, posix_spawn_file_actions_t& fa) {
    return posix_spawn_file_actions_addclosefrom_np(&fa, STDERR_FILENO + 1) == 0;
}

char** current_environment() { return environ; }

} // namespace posix
} // namespace platform
