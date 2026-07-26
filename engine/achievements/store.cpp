#include "store.hpp"
#include "tracker.hpp"

#include <cstdio>

#include "platform_fs.hpp"

namespace ach {

std::string temp_path_for(const std::string& path) { return path + ".tmp"; }

bool write_atomic(const std::string& path, const uint8_t* data, std::size_t size) {
    const std::string tmp = temp_path_for(path);
    std::FILE* f = platform::open_file(tmp, "wb");
    if (f == nullptr) return false;
    const bool written = size == 0 || std::fwrite(data, 1, size, f) == size;
    const bool synced = written && platform::sync_file(f);
    const bool closed = std::fclose(f) == 0;
    if (!written || !synced || !closed) {
        platform::remove_file(tmp);
        return false;
    }
    if (!platform::replace_file(tmp, path)) {
        platform::remove_file(tmp);
        return false;
    }
    platform::sync_dir_of(path);
    return true;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out, std::size_t max_bytes) {
    std::FILE* f = platform::open_file(path, "rb");
    if (f == nullptr) return false;
    out.clear();
    uint8_t buf[4096];
    std::size_t n = 0;
    bool ok = true;
    while (ok && (n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (out.size() + n > max_bytes) ok = false;
        else out.insert(out.end(), buf, buf + n);
    }
    ok = ok && std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) out.clear();
    return ok;
}

bool LocalStore::save(const Tracker& tr) const {
    Snapshot snap;
    tr.snapshot(snap);
    std::vector<uint8_t> bytes;
    encode(snap, bytes);
    return write_atomic(path_, bytes.data(), bytes.size());
}

bool LocalStore::load(Tracker& tr, DecodeResult* why) const {
    std::vector<uint8_t> bytes;
    if (!read_file(path_, bytes)) {
        if (why) *why = platform::file_exists(path_) ? DecodeResult::Unreadable : DecodeResult::Missing;
        return false;
    }
    Snapshot snap;
    const DecodeResult r = decode(bytes.data(), bytes.size(), snap);
    if (why) *why = r;
    if (r != DecodeResult::Ok) return false;
    // Возврат не проверяем намеренно: неизвестные записи Tracker уносит с собой и вернёт в
    // следующий snapshot(), так что загрузка бездефектна. Их число — через Tracker::carried_count().
    tr.restore(snap);
    return true;
}

} // namespace ach
