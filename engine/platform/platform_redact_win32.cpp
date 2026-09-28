#include "platform_env.hpp"
#include "platform_redact.hpp"

namespace platform {

std::string redact_home(const std::string& path) {
    std::string home;
    return env_var("USERPROFILE", home) ? redact_home(path, home, PathRules::Windows) : path;
}

} // namespace platform
