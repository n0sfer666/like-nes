#include "platform_path.hpp"

namespace platform {

bool is_sep(char c) { return c == '/'; }

bool is_absolute(const std::string& path) { return !path.empty() && path[0] == '/'; }

size_t root_len(const std::string& path) {
    return path.empty() || !is_sep(path[0]) ? 0 : 1;
}

} // namespace platform
