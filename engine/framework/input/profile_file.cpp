#include "profile_file.hpp"

#include "input_types.hpp"

namespace framework::input {

std::string profile_file(const std::string& base, int player) {
    if (player < 0 || player >= ::input::MAX_PLAYERS) return std::string();
    if (player == 0) return base;
    const std::size_t slash = base.find_last_of("/\\");
    const std::size_t stem = slash == std::string::npos ? 0 : slash + 1;
    std::size_t dot = base.find_last_of('.');
    if (dot == std::string::npos || dot <= stem) dot = base.size();
    return base.substr(0, dot) + "-p" + std::to_string(player + 1) + base.substr(dot);
}

} // namespace framework::input
