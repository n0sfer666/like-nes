#include "save_backend.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <utility>

#include "platform_fs.hpp"
#include "platform_path.hpp"
#include "save_format.hpp"
#include "save_io.hpp"

namespace save {
namespace detail {
bool valid_key(const std::string& key) {
    if (key.empty() || key.size() > 128 || key == "." || key == ".." || key.back() == '.' || key.ends_with(".tmp")) return false;
    if (!std::all_of(key.begin(), key.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
    })) return false;
    const std::string base = key.substr(0, key.find('.'));
    // Windows DOS-имена остаются устройствами даже с расширением и абсолютным путём.
    if (base == "con" || base == "prn" || base == "aux" || base == "nul") return false;
    if (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '1' && base[3] <= '9') return false;
    return true;
}
}

LocalBackend::LocalBackend(std::string root) {
    if (!root.empty() && root.find('\0') == std::string::npos && platform::is_absolute(root) && platform::ensure_dir(root))
        root_ = std::move(root);
}
std::string LocalBackend::path(const std::string& key) const {
    return root_.empty() || !detail::valid_key(key) ? std::string{} : root_ + "/" + key;
}

ReadResult LocalBackend::read(const std::string& key, std::vector<uint8_t>& out, size_t limit) {
    out.clear();
    const std::string file_path = path(key);
    if (file_path.empty() || !platform::is_dir(root_)) return ReadResult::Error;
    errno = 0;
    std::FILE* file = platform::open_file(file_path, "rb");
    if (!file) return errno == ENOENT ? ReadResult::Missing : ReadResult::Error;
    std::array<uint8_t, 4096> buffer;
    while (out.size() < limit) {
        const size_t chunk = std::min(buffer.size(), limit - out.size());
        const size_t n = std::fread(buffer.data(), 1, chunk, file);
        out.insert(out.end(), buffer.begin(), buffer.begin() + n);
        if (n < chunk) break;
    }
    const bool too_large = out.size() == limit && std::ferror(file) == 0 && std::fgetc(file) != EOF;
    const bool good = std::ferror(file) == 0;
    const bool closed = std::fclose(file) == 0;
    if (!good || !closed) { out.clear(); return ReadResult::Error; }
    return too_large ? ReadResult::TooLarge : ReadResult::Ok;
}
bool LocalBackend::write(const std::string& key, std::span<const uint8_t> bytes) {
    const std::string file_path = path(key);
    return !file_path.empty() && platform::write_atomic(file_path, bytes);
}
bool LocalBackend::list(std::vector<std::string>& keys) {
    keys.clear();
    if (root_.empty() || !platform::list_dir(root_, keys)) return false;
    keys.erase(std::remove_if(keys.begin(), keys.end(), [](const std::string& key) { return !detail::valid_key(key); }), keys.end());
    std::sort(keys.begin(), keys.end());
    return true;
}
MoveResult LocalBackend::move_new(const std::string& from, const std::string& to) {
    return detail::move_new_paths(path(from), path(to));
}

Selection select_record(std::span<const std::vector<uint8_t>> copies) {
    Selection selected;
    bool found = false, conflict = false;
    for (size_t i = 0; i < copies.size(); ++i) {
        RecordView view;
        if (decode_record(copies[i], view) == RecordResult::Corrupt) continue;
        if (!found || view.writes > selected.writes) {
            selected = {SelectionResult::Selected, i, view.writes}; found = true; conflict = false;
        } else if (view.writes == selected.writes && copies[i] != copies[selected.index]) conflict = true;
    }
    if (!found && !copies.empty()) selected.result = SelectionResult::Corrupt;
    if (conflict) { selected.result = SelectionResult::Conflict; selected.index = static_cast<size_t>(-1); }
    return selected;
}
} // namespace save
