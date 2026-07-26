#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bake.hpp"
#include "bundle_writer.hpp"
#include "hash.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

const char* const BUNDLE_PATH = "ach_bundle_test.bundle";

const char* const SOURCE =
    "stat | stat_kills\n"
    "ach | FIRST_BLOOD | progress | stat_kills | 1  | -      | First Blood | Kill one enemy\n"
    "ach | KILLER_10   | progress | stat_kills | 10 | -      | Killer      | Kill ten enemies\n"
    "ach | NO_DAMAGE   | bool     |            |    | hidden | Untouchable | Take no damage\n";

bool write_bundle_file(const std::string& path, const std::vector<uint8_t>& table, bool with_ach) {
    std::vector<asset::AssetInput> assets;
    asset::AssetInput filler;
    filler.guid = asset::fnv1a("filler", 6);
    filler.type = asset::AssetType::Raw;
    filler.codec = asset::Codec::Raw;
    filler.residency = asset::Residency::Mmap;
    filler.payload.assign(64, 0x5a);
    filler.uncompressed_size = 64;
    assets.push_back(std::move(filler));

    if (with_ach) {
        asset::AssetInput a;
        a.guid = asset::fnv1a("achievements", std::strlen("achievements"));
        a.type = asset::AssetType::Raw;
        a.codec = asset::Codec::Raw;
        a.residency = asset::Residency::Mmap;
        a.uncompressed_size = static_cast<uint32_t>(table.size());
        a.payload = table;
        assets.push_back(std::move(a));
    }

    const std::vector<uint8_t> bytes = asset::write_bundle(std::move(assets));
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return std::fclose(f) == 0 && ok;
}

std::vector<uint8_t> bake() {
    std::vector<uint8_t> table;
    ach::BakeError err;
    if (!ach::bake_manifest(SOURCE, table, err)) {
        ++failures;
        std::printf("  FAIL bake: line %d: %s\n", err.line, err.message.c_str());
    }
    return table;
}

} // namespace
