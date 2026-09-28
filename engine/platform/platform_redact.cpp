#include "platform_redact.hpp"

namespace platform {
namespace {

bool is_sep(char c, PathRules rules) {
    return c == '/' || (rules == PathRules::Windows && c == '\\');
}

char fold(char c, PathRules rules) {
    if (rules == PathRules::Posix) return c;
    if (c == '\\') return '/';
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

} // namespace

std::string redact_home(const std::string& path, const std::string& home, PathRules rules) {
    size_t n = home.size();
    while (n > 0 && is_sep(home[n - 1], rules)) --n;
    const bool root = n == 0 || (rules == PathRules::Windows && n == 2 && home[1] == ':');
    if (root || path.size() < n) return path;
    for (size_t i = 0; i < n; ++i)
        if (fold(path[i], rules) != fold(home[i], rules)) return path;
    if (path.size() > n && !is_sep(path[n], rules)) return path;
    return "~" + path.substr(n);
}

} // namespace platform
