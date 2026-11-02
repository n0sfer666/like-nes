#include "platform_fs.hpp"

#include <unistd.h>

namespace platform {
bool sync_file(std::FILE* file) {
    return file && std::fflush(file) == 0 && ::fsync(fileno(file)) == 0;
}
} // namespace platform
