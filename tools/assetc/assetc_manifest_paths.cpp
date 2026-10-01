#include "assetc_manifest.hpp"

#include "platform_fs.hpp"

namespace asset::manifest {
namespace {

bool same_ignoring_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

} // namespace

// Путь лексически проверен `parse`: сегменты непустые, без `.` и `..`, разделитель `/`.
bool resolve(const std::string& base_dir, const std::string& rel, std::string& full,
             std::string& error) {
    std::string dir = base_dir;
    std::vector<std::string> names;
    size_t start = 0;
    while (start < rel.size()) {
        const size_t slash = rel.find('/', start);
        const size_t end = slash == std::string::npos ? rel.size() : slash;
        const std::string seg = rel.substr(start, end - start);
        if (!platform::list_dir(dir, names)) {
            error = "cannot list directory " + dir;
            return false;
        }
        bool exact = false;
        std::string near;
        for (const std::string& n : names) {
            if (n == seg) exact = true;
            else if (same_ignoring_case(n, seg)) near = n;
        }
        if (!exact) {
            error = near.empty() ? "'" + rel + "' not found: no '" + seg + "' in " + dir
                                 : "'" + rel + "': '" + seg + "' is spelled '" + near +
                                       "' on disk; match the case exactly";
            return false;
        }
        dir += "/" + seg;
        start = end + 1;
    }
    if (platform::is_dir(dir)) {
        error = "'" + rel + "' is a directory, not a file";
        return false;
    }
    full = dir;
    return true;
}

} // namespace asset::manifest
