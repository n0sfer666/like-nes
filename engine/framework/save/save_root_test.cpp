#include "save_root.hpp"

#include <cstdio>

namespace {
std::string shell, env, app_seen;
bool configured, has_env;
int env_calls, os_calls;
}
namespace platform {
const std::string* save_dir_override() { return configured ? &shell : nullptr; }
bool env_var(const char* name, std::string& out) {
    ++env_calls;
    if (std::string(name) != "LIKENES_SAVE_DIR" || !has_env) return false;
    out = env; return true;
}
std::string user_data_dir(const std::string& app) { ++os_calls; app_seen = app; return "os-root"; }
}
int main() {
    int fails = 0;
    auto check = [&](bool good) { if (!good) ++fails; };
    check(save::detail::save_root("my-app") == "os-root" && app_seen == "my-app");
    has_env = true; env = "explicit-env";
    check(save::detail::save_root("my-app") == env && os_calls == 1);
    env.clear();
    check(save::detail::save_root("my-app") == "os-root" && os_calls == 2);
    env = "relative-refused-by-backend";
    check(save::detail::save_root("my-app") == env && os_calls == 2);
    configured = true; shell = "shell-root";
    const int before = env_calls;
    check(save::detail::save_root("my-app") == shell && env_calls == before && os_calls == 2);
    shell.clear();
    check(save::detail::save_root("my-app").empty() && env_calls == before && os_calls == 2);
    std::puts(fails ? "save-root: FAIL" : "save-root: PASS shell incl refusal > explicit env > OS(app), no override fallback");
    return fails ? 1 : 0;
}
