#include "platform_env.hpp"
#include "platform_redact.hpp"

namespace platform {

std::string redact_home(const std::string& path) {
    std::string home;
    return env_var("HOME", home) ? redact_home(path, home, PathRules::Posix) : path;
}

} // namespace platform
