#include "store.hpp"
#include "tracker.hpp"

#include <cstdio>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ach {
namespace {

bool sync_stream(std::FILE* f) {
    if (std::fflush(f) != 0) return false;
#ifdef _WIN32
    return _commit(_fileno(f)) == 0;
#else
    return ::fsync(fileno(f)) == 0;
#endif
}

void sync_dir(const std::string& path) {
#ifndef _WIN32
    const std::size_t slash = path.find_last_of('/');
    const std::string dir = slash == std::string::npos ? std::string(".") : path.substr(0, slash);
    const int fd = ::open(dir.c_str(), O_RDONLY);
    if (fd < 0) return;
    ::fsync(fd);
    ::close(fd);
#else
    (void)path;
#endif
}

bool file_exists(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::fclose(f);
    return true;
}

bool replace_file(const std::string& from, const std::string& to) {
#ifdef _WIN32
    return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return std::rename(from.c_str(), to.c_str()) == 0;
#endif
}

} // namespace

std::string temp_path_for(const std::string& path) { return path + ".tmp"; }

bool write_atomic(const std::string& path, const uint8_t* data, std::size_t size) {
    const std::string tmp = temp_path_for(path);
    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) return false;
    const bool written = size == 0 || std::fwrite(data, 1, size, f) == size;
    const bool synced = written && sync_stream(f);
    const bool closed = std::fclose(f) == 0;
    if (!written || !synced || !closed) {
        std::remove(tmp.c_str());
        return false;
    }
    if (!replace_file(tmp, path)) {
        std::remove(tmp.c_str());
        return false;
    }
    sync_dir(path);
    return true;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out, std::size_t max_bytes) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
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
        if (why) *why = file_exists(path_) ? DecodeResult::Unreadable : DecodeResult::Missing;
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
