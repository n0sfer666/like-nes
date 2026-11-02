#include "store.hpp"
#include "tracker.hpp"

#include <cstdio>
#include <cerrno>

#include "platform_fs.hpp"
#include "platform_path.hpp"

namespace ach {

std::string temp_path_for(const std::string& path) { return path + ".tmp"; }

bool write_atomic(const std::string& path, const uint8_t* data, std::size_t size) {
    if (size > 0 && !data) return false;
    return platform::write_atomic(path, {data, size});
}

namespace {
DecodeResult read_checked(const std::string& path, std::vector<uint8_t>& out, std::size_t max_bytes) {
    out.clear();
    if (path.empty() || path.find('\0') != std::string::npos || !platform::is_dir(platform::parent_dir(path))) return DecodeResult::Unreadable;
    errno = 0;
    std::FILE* f = platform::open_file(path, "rb");
    if (f == nullptr) return errno == ENOENT ? DecodeResult::Missing : DecodeResult::Unreadable;
    uint8_t buf[4096];
    std::size_t n = 0;
    bool ok = true;
    while (ok && (n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (out.size() + n > max_bytes) ok = false;
        else out.insert(out.end(), buf, buf + n);
    }
    ok = ok && std::ferror(f) == 0;
    const bool closed = std::fclose(f) == 0;
    ok = ok && closed;
    if (!ok) out.clear();
    return ok ? DecodeResult::Ok : DecodeResult::Unreadable;
}
}
bool read_file(const std::string& path, std::vector<uint8_t>& out, std::size_t max_bytes) {
    return read_checked(path, out, max_bytes) == DecodeResult::Ok;
}

bool LocalStore::save(const Tracker& tr) const {
    Snapshot snap;
    tr.snapshot(snap);
    std::vector<uint8_t> bytes;
    encode(snap, bytes);
    return write_atomic(path_, bytes.data(), bytes.size());
}

bool LocalStore::read(Snapshot& snap, DecodeResult* why) const {
    snap = {};
    std::vector<uint8_t> bytes;
    DecodeResult r = read_checked(path_, bytes, STATE_MAX_SIZE);
    if (r == DecodeResult::Ok) r = decode(bytes.data(), bytes.size(), snap);
    if (why) *why = r;
    return r == DecodeResult::Ok;
}
bool LocalStore::load(Tracker& tr, DecodeResult* why) const {
    Snapshot snap;
    if (!read(snap, why)) return false;
    // Возврат не проверяем намеренно: неизвестные записи Tracker уносит с собой и вернёт в
    // следующий snapshot(), так что загрузка бездефектна. Их число — через Tracker::carried_count().
    // Бездефектна она до потолка переноса (Tracker::MAX_CARRIED): сверх него записи отбрасываются,
    // и сколько именно — спрашивается у Tracker::dropped_count(). Файл такого размера законным
    // путём не берётся, но «загрузка ничего не теряет» перестало быть безусловным, и молчать об
    // этом здесь значило бы оставить читателю вчерашнее обещание. Потеря НЕОБРАТИМА: отброшенного
    // нет в snapshot(), а первый же save() перезапишет файл этим снимком — то есть записи исчезнут
    // не только из памяти, но и с диска.
    tr.restore(snap);
    return true;
}

} // namespace ach
