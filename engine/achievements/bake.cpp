#include "bake.hpp"
#include "bake_rows.hpp"
#include "manifest.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

namespace ach {
namespace {

uint32_t intern(std::map<std::string, uint32_t>& seen, std::string& blob, const std::string& s) {
    auto it = seen.find(s);
    if (it != seen.end()) return it->second;
    const uint32_t off = static_cast<uint32_t>(blob.size());
    blob.append(s);
    blob.push_back('\0');
    seen.emplace(s, off);
    return off;
}

void put(std::vector<uint8_t>& out, const void* p, std::size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    out.insert(out.end(), b, b + n);
}

} // namespace

bool bake_manifest(const std::string& text, std::vector<uint8_t>& out, BakeError& err) {
    std::vector<Row> rows;
    std::vector<std::string> stat_keys;
    if (!parse_manifest(text, rows, stat_keys, err)) return false;

    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.def.id < b.def.id; });
    std::sort(stat_keys.begin(), stat_keys.end(), [](const std::string& a, const std::string& b) {
        return hash_key(a.c_str()) < hash_key(b.c_str());
    });

    std::map<std::string, uint32_t> seen;
    std::string blob;
    std::vector<ManifestDef> defs;
    defs.reserve(rows.size());
    for (const Row& r : rows) {
        ManifestDef d{};
        d.def = r.def;
        d.key_off = intern(seen, blob, r.key);
        d.name_off = intern(seen, blob, r.name);
        d.desc_off = intern(seen, blob, r.desc);
        defs.push_back(d);
    }
    std::vector<ManifestStat> stats;
    stats.reserve(stat_keys.size());
    for (const std::string& k : stat_keys) {
        ManifestStat s{};
        s.id = hash_key(k.c_str());
        s.key_off = intern(seen, blob, k);
        stats.push_back(s);
    }

    ManifestHeader h{};
    std::memcpy(h.magic, MANIFEST_MAGIC, 4);
    h.version = MANIFEST_VERSION;
    h.ach_count = static_cast<uint32_t>(defs.size());
    h.stat_count = static_cast<uint32_t>(stats.size());
    h.defs_offset = sizeof(ManifestHeader);
    h.stats_offset = h.defs_offset + static_cast<uint32_t>(defs.size() * sizeof(ManifestDef));
    h.strings_offset = h.stats_offset + static_cast<uint32_t>(stats.size() * sizeof(ManifestStat));
    h.total_size = h.strings_offset + static_cast<uint32_t>(blob.size());

    out.clear();
    out.reserve(h.total_size);
    put(out, &h, sizeof(h));
    for (const ManifestDef& d : defs) put(out, &d, sizeof(d));
    for (const ManifestStat& s : stats) put(out, &s, sizeof(s));
    put(out, blob.data(), blob.size());
    return true;
}

bool bake_manifest_file(const std::string& path, std::vector<uint8_t>& out, BakeError& err) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        err.line = 0;
        err.message = "cannot open " + path;
        return false;
    }
    std::string text;
    char buf[4096];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) {
        err.line = 0;
        err.message = "read error " + path;
        return false;
    }
    return bake_manifest(text, out, err);
}

} // namespace ach
