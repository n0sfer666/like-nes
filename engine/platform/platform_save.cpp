#include "platform_save.hpp"

#include <utility>

#include "platform_path.hpp"

namespace platform {
namespace {
bool configured = false;
std::string directory;
}
bool set_save_dir(std::string path) {
    configured = true;
    const bool accepted = !path.empty() && path.find('\0') == std::string::npos && is_absolute(path);
    directory = accepted ? std::move(path) : std::string{};
    return accepted;
}
const std::string* save_dir_override() { return configured ? &directory : nullptr; }
}
