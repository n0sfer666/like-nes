#include "platform_fs.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace platform {
bool sync_file(std::FILE* file) {
    // fsync на Apple не гарантирует сброс кэша накопителя до последующего rename.
    // Не скрываем отказ full sync слабой заменой: вызывающий должен сохранить старый сейв.
    return file && std::fflush(file) == 0 && ::fcntl(fileno(file), F_FULLFSYNC) == 0;
}
} // namespace platform
