#include "save.hpp"

#include "platform_fs.hpp"

bool save::write_atomic(const std::string& path, std::span<const uint8_t> bytes) {
    return platform::write_atomic(path, bytes);
}
