#include "probe_persistence.hpp"

#include <cstdio>

#include "platform_fs.hpp"
#include "platform_redact.hpp"
#include "rebind_store.hpp"
#include "save.hpp"

namespace framework::input {
namespace {
constexpr const char* KEY = "controls_probe.txt";
bool preserve_late_profile(const std::string& path) {
    if (!platform::file_exists(path)) return true;
    save::LocalBackend backend(save::dir());
    std::vector<std::string> keys;
    if (!backend.list(keys)) return false;
    for (size_t k = 0; k <= keys.size(); ++k) {
        const std::string backup = std::string(KEY) + "." + std::to_string(k) + ".pre";
        const auto moved = backend.move_new(KEY, backup);
        if (moved == save::MoveResult::Error) return false;
        if (moved == save::MoveResult::Moved) {
            std::printf("[probe] late profile retained: %s\n", platform::redact_home(save::dir() + "/" + backup).c_str());
            return true;
        }
    }
    return false;
}
}
bool save_probe_profile(const RebindStore& store, const std::string& preset, bool& prior_loaded, std::string& path) {
    path = save::file_path(KEY); // Retries the intended root; no cached empty path after init refusal.
    if (path.empty() || (!prior_loaded && !preserve_late_profile(path)) || !store.save(path, preset)) return false;
    prior_loaded = true;
    return true;
}
}
