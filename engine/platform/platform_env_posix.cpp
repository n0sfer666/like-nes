#include "platform_env.hpp"

#include <cstdlib>

namespace platform {

bool env_var(const char* name, std::string& out) {
    const char* v = std::getenv(name);
    if (v == nullptr) return false;
    out = v;
    return true;
}

bool env_put(const char* name, const char* value) {
    if (value == nullptr) return ::unsetenv(name) == 0;
    return ::setenv(name, value, 1) == 0;
}

} // namespace platform
