#pragma once
#include "def.hpp"
#include <cstddef>
#include <cstdint>

namespace ach {

class Registry;

constexpr uint8_t MANIFEST_MAGIC[4] = {'L', 'N', 'A', 'M'};
constexpr uint32_t MANIFEST_VERSION = 1;

struct ManifestHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t ach_count;
    uint32_t stat_count;
    uint32_t defs_offset;
    uint32_t stats_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(ManifestHeader) == 32, "ManifestHeader layout pinned (zero-parse ABI)");

struct ManifestDef {
    Def def;
    uint32_t key_off;
    uint32_t name_off;
    uint32_t desc_off;
    uint32_t reserved;
};
static_assert(sizeof(ManifestDef) == 48, "ManifestDef layout pinned (zero-parse ABI)");

struct ManifestStat {
    Id id;
    uint32_t key_off;
    uint32_t reserved;
};
static_assert(sizeof(ManifestStat) == 16, "ManifestStat layout pinned (zero-parse ABI)");

constexpr Id MANIFEST_ASSET_GUID = hash_key("achievements");

enum class LoadResult : uint32_t {
    Ok = 0,
    TooShort = 1,
    BadMagic = 2,
    BadVersion = 3,
    BadLayout = 4,
    BadString = 5,
    Duplicate = 6,
    BadSpec = 7,
};

LoadResult load_manifest(Registry& reg, const void* base, std::size_t size);
const char* load_reason(LoadResult r);

} // namespace ach
