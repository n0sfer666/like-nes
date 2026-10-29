#include "assetc_tool.hpp"

#include <algorithm>

namespace asset {

std::string tool_name(const char* argv0) {
    const std::string path = argv0 ? argv0 : "assetc";
    const size_t slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    const bool exe = name.size() > 4 && std::equal(name.end() - 4, name.end(), ".exe",
                                                   [&](char a, char b) { return lower(a) == b; });
    if (exe) name.resize(name.size() - 4);
    return name;
}

} // namespace asset
