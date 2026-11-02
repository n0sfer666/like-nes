#include "save_root.hpp"

#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "platform_save.hpp"

namespace save::detail {
std::string save_root(const std::string& app) {
    if (const auto* shell = platform::save_dir_override()) return *shell;
    std::string env;
    if (platform::env_var("LIKENES_SAVE_DIR", env) && !env.empty()) return env;
    return platform::user_data_dir(app);
}
}
