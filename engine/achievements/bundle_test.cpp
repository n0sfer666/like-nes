#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bundle_fixture.hpp"
#include "bundle_source.hpp"
#include "platform_args.hpp"
#include "registry.hpp"
#include "tracker.hpp"

namespace {

void test_guid_contract() {
    check(asset::fnv1a("achievements", std::strlen("achievements")) == ach::MANIFEST_ASSET_GUID,
          "bundle guid and manifest guid agree");
}

void test_runtime_seam() {
    const std::vector<uint8_t> table = bake();
    check(write_bundle_file(BUNDLE_PATH, table, true), "bundle written");

    ach::BundleSource src;   // объявлен раньше реестра → умирает позже: строки живут в его маппинге
    ach::Registry reg;
    const ach::SourceStatus st = src.open(reg, BUNDLE_PATH);
    check(st.result == ach::SourceResult::Ok, ach::source_reason(st));
    check(reg.entries().size() == 3, "three achievements from bundle");
    check(reg.stats().size() == 1, "one stat from bundle");

    const ach::Entry* e = reg.find(ach::hash_key("KILLER_10"));
    check(e != nullptr, "entry found");
    if (e != nullptr) {
        check(std::strcmp(e->name, "Killer") == 0, "name readable from mapped bundle");
        check(e->def.target == 10, "target readable from mapped bundle");
    }

    const ach::SourceStatus again = src.open(reg, BUNDLE_PATH);
    check(again.result == ach::SourceResult::AlreadyOpen, "second open rejected while mapped");
    check(reg.entries().size() == 3, "rejected open leaves the registry alone");
    check(src.is_open(), "rejected open keeps the live mapping");

    ach::Tracker tr(reg);
    tr.add_stat(ach::hash_key("stat_kills"), 10);
    check(tr.unlocked(ach::hash_key("KILLER_10")), "bundle-defined threshold fires");
    check(tr.events().size() == 2, "first blood and killer");
}

void test_runtime_merge() {
    const std::vector<uint8_t> table = bake();
    check(write_bundle_file(BUNDLE_PATH, table, true), "bundle written for merge");

    ach::BundleSource src;
    ach::Registry reg;
    check(src.open(reg, BUNDLE_PATH).result == ach::SourceResult::Ok, "bundle loaded for merge");
    check(reg.define({"PROC_RUN_7", "Lucky Seven", "", ach::Kind::Progress, "stat_kills", 7, 0}) ==
              ach::DefineResult::Ok, "runtime definition on top of bundle");
    check(reg.define({"KILLER_10", "dup", "", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Duplicate, "runtime cannot redefine baked id");
    check(reg.entries().size() == 4, "merged set");

    ach::Tracker tr(reg);
    tr.add_stat(ach::hash_key("stat_kills"), 7);
    check(tr.unlocked(ach::hash_key("PROC_RUN_7")), "runtime achievement shares baked stat");
    check(!tr.unlocked(ach::hash_key("KILLER_10")), "baked threshold not yet reached");
}

void test_missing_sources() {
    ach::BundleSource src;
    ach::Registry reg;
    const ach::SourceStatus none = src.open(reg, "ach_bundle_test_absent.bundle");
    check(none.result == ach::SourceResult::NoBundle, "absent bundle reported");
    check(!src.is_open(), "no mapping kept on failure");

    check(write_bundle_file(BUNDLE_PATH, std::vector<uint8_t>(), false), "bundle without table");
    const ach::SourceStatus empty = src.open(reg, BUNDLE_PATH);
    check(empty.result == ach::SourceResult::NoAsset, "missing asset reported");
    check(reg.entries().empty(), "registry untouched on failure");

    std::vector<uint8_t> table = bake();
    table[6] = 0x7f;
    check(write_bundle_file(BUNDLE_PATH, table, true), "bundle with corrupt table");
    const ach::SourceStatus bad = src.open(reg, BUNDLE_PATH);
    check(bad.result == ach::SourceResult::BadManifest, "corrupt table rejected");
    check(!src.is_open(), "no mapping kept on corrupt table");
}

// Щель между гейтами: отказ на N-й записи, когда N-1 уже adopted. Реестр держит указатели внутрь
// mmap-региона, который open() тут же размапит — частичный результат обязан откатываться целиком.
void test_partial_failure_rolls_back() {
    std::vector<uint8_t> table = bake();
    if (table.empty()) return;
    ach::ManifestHeader h{};
    std::memcpy(&h, table.data(), sizeof(h));
    const std::size_t last = h.defs_offset + (h.ach_count - 1) * sizeof(ach::ManifestDef) +
                             offsetof(ach::ManifestDef, desc_off);
    const uint32_t unreachable = 0xfffffff0u;
    std::memcpy(table.data() + last, &unreachable, sizeof(unreachable));
    check(write_bundle_file(BUNDLE_PATH, table, true), "bundle with a broken last entry");

    ach::BundleSource src;
    ach::Registry reg;
    check(reg.define({"PROC_RUN_7", "Lucky Seven", "", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Ok, "runtime definition before the failed load");

    const ach::SourceStatus st = src.open(reg, BUNDLE_PATH);
    check(st.result == ach::SourceResult::BadManifest && st.load == ach::LoadResult::BadString,
          "broken string offset rejected");
    check(!src.is_open(), "no mapping kept after a partial load");
    check(reg.entries().size() == 1 && reg.stats().empty(),
          "partial load rolled back: nothing points into the unmapped bundle");
    check(std::strcmp(reg.entries()[0].key, "PROC_RUN_7") == 0, "runtime definitions survive");

    ach::Tracker tr(reg);
    check(tr.unlocked_count() == 0, "rolled-back registry is still usable");
}

void test_shipped_bundle(const char* path) {
    ach::BundleSource src;
    ach::Registry reg;
    const ach::SourceStatus st = src.open(reg, path);
    check(st.result == ach::SourceResult::Ok, ach::source_reason(st));
    check(!reg.entries().empty(), "shipped bundle carries achievements");
    for (const ach::Entry& e : reg.entries()) {
        std::printf("  %s: %s (%s)\n", e.key, e.name, e.desc);
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("achievements bundle seam\n");
    if (argc > 1) {
        test_shipped_bundle(argv[1]);
        std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
        return failures == 0 ? 0 : 1;
    }
    test_guid_contract();
    test_runtime_seam();
    test_runtime_merge();
    test_missing_sources();
    test_partial_failure_rolls_back();
    std::remove(BUNDLE_PATH);
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
