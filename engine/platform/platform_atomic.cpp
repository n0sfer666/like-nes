#include "platform_fs.hpp"

namespace platform {

bool write_atomic(const std::string& path, std::span<const uint8_t> bytes) {
    // NUL обрезал бы имя до суффикса .tmp и открыл бы саму цель с усечением.
    if (path.empty() || path.find('\0') != std::string::npos) return false;
    const std::string temp = path + ".tmp";
    // Stale symlink/hardlink нельзя открывать с усечением: он может ссылаться на сам сейв.
    if (!remove_file(temp)) return false;
    std::FILE* file = open_file(temp, "wb");
    if (!file) return false;
    const bool written = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    const bool synced = written && sync_file(file);
    const bool closed = std::fclose(file) == 0;
    if (!written || !synced || !closed || !replace_file(temp, path)) {
        remove_file(temp);
        return false;
    }
    return sync_dir_of(path);
}

} // namespace platform
