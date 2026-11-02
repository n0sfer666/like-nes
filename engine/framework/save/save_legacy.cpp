#include "save.hpp"

#include <algorithm>

#include "platform_fs.hpp"
#include "platform_path.hpp"
#include "save_io.hpp"

namespace save {
bool migrate_legacy(const std::string& source_dir, std::span<const std::string> filenames) {
    if (source_dir.empty()) return true;
    if (dir().empty() || !platform::is_dir(dir()) || source_dir.find('\0') != std::string::npos ||
        !platform::is_absolute(source_dir) ||
        std::any_of(filenames.begin(), filenames.end(), [](const auto& name) { return !detail::valid_key(name); })) return false;
    if (source_dir == dir()) return true;
    bool good = true;
    for (const auto& name : filenames) {
        const std::string from = source_dir + "/" + name;
        if (!platform::file_exists(from)) { if (platform::is_dir(from)) good = false; continue; }
        const auto moved = detail::move_new_paths(from, dir() + "/" + name);
        if (moved == MoveResult::Error) good = false;
    }
    return good;
}
}
