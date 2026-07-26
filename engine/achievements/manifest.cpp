#include "manifest.hpp"
#include "registry.hpp"

#include <cstring>

namespace ach {
namespace {

bool region_ok(uint32_t offset, uint64_t bytes, std::size_t size) {
    return static_cast<uint64_t>(offset) + bytes <= size;
}

bool aligned(uint32_t offset, uint32_t to) { return offset % to == 0; }

const char* string_at(const uint8_t* base, const ManifestHeader& h, std::size_t size,
                      uint32_t off) {
    const uint64_t start = static_cast<uint64_t>(h.strings_offset) + off;
    if (start >= size) return nullptr;
    const char* s = reinterpret_cast<const char*>(base + start);
    const std::size_t room = size - static_cast<std::size_t>(start);
    return std::memchr(s, '\0', room) == nullptr ? nullptr : s;
}

} // namespace

LoadResult load_manifest(Registry& reg, const void* base_ptr, std::size_t size) {
    const uint8_t* base = static_cast<const uint8_t*>(base_ptr);
    if (base == nullptr || size < sizeof(ManifestHeader)) return LoadResult::TooShort;

    ManifestHeader h{};
    std::memcpy(&h, base, sizeof(h));
    if (std::memcmp(h.magic, MANIFEST_MAGIC, 4) != 0) return LoadResult::BadMagic;
    if (h.version != MANIFEST_VERSION) return LoadResult::BadVersion;
    if (h.total_size != size) return LoadResult::BadLayout;
    if (!aligned(h.defs_offset, 8) || !aligned(h.stats_offset, 8)) return LoadResult::BadLayout;
    if (!region_ok(h.defs_offset, static_cast<uint64_t>(h.ach_count) * sizeof(ManifestDef), size) ||
        !region_ok(h.stats_offset, static_cast<uint64_t>(h.stat_count) * sizeof(ManifestStat), size) ||
        !region_ok(h.strings_offset, 0, size)) {
        return LoadResult::BadLayout;
    }

    Registry::Transaction tx(reg);
    for (uint32_t i = 0; i < h.stat_count; ++i) {
        ManifestStat s{};
        std::memcpy(&s, base + h.stats_offset + i * sizeof(ManifestStat), sizeof(s));
        const char* key = string_at(base, h, size, s.key_off);
        if (key == nullptr) return LoadResult::BadString;
        const DefineResult r = reg.adopt_stat(s.id, key);
        if (r == DefineResult::Duplicate) return LoadResult::Duplicate;
        if (r != DefineResult::Ok) return LoadResult::BadSpec;
    }

    for (uint32_t i = 0; i < h.ach_count; ++i) {
        ManifestDef d{};
        std::memcpy(&d, base + h.defs_offset + i * sizeof(ManifestDef), sizeof(d));
        const char* key = string_at(base, h, size, d.key_off);
        const char* name = string_at(base, h, size, d.name_off);
        const char* desc = string_at(base, h, size, d.desc_off);
        if (key == nullptr || name == nullptr || desc == nullptr) return LoadResult::BadString;
        const DefineResult r = reg.adopt(d.def, key, name, desc);
        if (r == DefineResult::Duplicate) return LoadResult::Duplicate;
        if (r != DefineResult::Ok) return LoadResult::BadSpec;
    }
    tx.commit();
    return LoadResult::Ok;
}

const char* load_reason(LoadResult r) {
    switch (r) {
        case LoadResult::Ok: return "ok";
        case LoadResult::TooShort: return "too short";
        case LoadResult::BadMagic: return "bad magic";
        case LoadResult::BadVersion: return "bad version";
        case LoadResult::BadLayout: return "bad layout";
        case LoadResult::BadString: return "bad string";
        case LoadResult::Duplicate: return "duplicate id";
        case LoadResult::BadSpec: return "invalid definition";
    }
    return "unknown";
}

} // namespace ach
