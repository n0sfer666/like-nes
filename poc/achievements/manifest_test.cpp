#include "bake.hpp"
#include "manifest.hpp"
#include "registry.hpp"
#include "tracker.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

const char* const SOURCE =
    "# achievements of the sample game\n"
    "stat | stat_kills\n"
    "stat | stat_score\n"
    "\n"
    "ach | FIRST_BLOOD | progress | stat_kills | 1   | -      | First Blood | Kill one enemy\n"
    "ach | KILLER_10   | progress | stat_kills | 10  | -      | Killer      | Kill ten enemies\n"
    "ach | SCORE_500   | progress | stat_score | 500 | -      | High Scorer | Reach 500 points\n"
    "ach | BOSS_DOWN   | bool     |            | 0   | -      | Boss Down   | Defeat the boss\n"
    "ach | NO_DAMAGE   | bool     |            |     | hidden | Untouchable | Take no damage\n";

const char* const SHUFFLED =
    "ach | SCORE_500   | progress | stat_score | 500 | -      | High Scorer | Reach 500 points\n"
    "stat | stat_score\n"
    "ach | NO_DAMAGE   | bool     |            |     | hidden | Untouchable | Take no damage\n"
    "stat | stat_kills\n"
    "ach | BOSS_DOWN   | bool     |            | 0   | -      | Boss Down   | Defeat the boss\n"
    "ach | KILLER_10   | progress | stat_kills | 10  | -      | Killer      | Kill ten enemies\n"
    "ach | FIRST_BLOOD | progress | stat_kills | 1   | -      | First Blood | Kill one enemy\n";

std::vector<uint8_t> bake_ok(const char* text, const char* what) {
    std::vector<uint8_t> bytes;
    ach::BakeError err;
    if (!ach::bake_manifest(text, bytes, err)) {
        ++failures;
        std::printf("  FAIL %s: line %d: %s\n", what, err.line, err.message.c_str());
    }
    return bytes;
}

void expect_bake_error(const char* text, int line, const char* what) {
    std::vector<uint8_t> bytes;
    ach::BakeError err;
    const bool ok = ach::bake_manifest(text, bytes, err);
    check(!ok, what);
    if (!ok) check(err.line == line, what);
    if (!ok && err.line != line) std::printf("    (line %d: %s)\n", err.line, err.message.c_str());
}

void test_seam() {
    const std::vector<uint8_t> bytes = bake_ok(SOURCE, "bake source");
    check(!bytes.empty(), "bake produced bytes");
    if (bytes.empty()) return;

    ach::Registry reg;
    const ach::LoadResult r = ach::load_manifest(reg, bytes.data(), bytes.size());
    check(r == ach::LoadResult::Ok, ach::load_reason(r));
    check(reg.entries().size() == 5, "five achievements");
    check(reg.stats().size() == 2, "two stats");

    const ach::Entry* e = reg.find(ach::hash_key("SCORE_500"));
    check(e != nullptr, "find baked entry");
    if (e != nullptr) {
        check(e->def.stat == ach::hash_key("stat_score"), "stat bound");
        check(e->def.target == 500, "target baked");
        check(e->def.kind == static_cast<uint32_t>(ach::Kind::Progress), "kind baked");
        check(std::strcmp(e->key, "SCORE_500") == 0, "key in place");
        check(std::strcmp(e->name, "High Scorer") == 0, "name in place");
        check(std::strcmp(e->desc, "Reach 500 points") == 0, "desc in place");
        const uint8_t* inside = reinterpret_cast<const uint8_t*>(e->key);
        check(inside >= bytes.data() && inside < bytes.data() + bytes.size(), "strings zero-copy");
    }
    const ach::Entry* hidden = reg.find(ach::hash_key("NO_DAMAGE"));
    check(hidden != nullptr && hidden->def.flags == ach::FLAG_HIDDEN, "hidden flag baked");

    ach::Tracker tr(reg);
    tr.add_stat(ach::hash_key("stat_kills"), 10);
    check(tr.unlocked(ach::hash_key("KILLER_10")), "baked threshold works");
}

void test_determinism() {
    const std::vector<uint8_t> a = bake_ok(SOURCE, "bake a");
    const std::vector<uint8_t> b = bake_ok(SOURCE, "bake b");
    const std::vector<uint8_t> c = bake_ok(SHUFFLED, "bake shuffled");
    check(a == b, "bake is byte-reproducible");
    check(a == c, "source order does not change bytes");
}

void test_source_errors() {
    expect_bake_error("stat | stat_kills\nstat | stat_kills\n", 2, "duplicate stat");
    expect_bake_error("stat | s\nach | A | bool | | 0 | - | A | a\nach | A | bool | | 0 | - | A | a\n",
                      3, "duplicate achievement");
    expect_bake_error("ach | A | progress | s | 5 | - | A | a\n", 1, "undeclared stat");
    expect_bake_error("stat | s\nach | A | progress | s | 0 | - | A | a\n", 2, "zero target");
    expect_bake_error("stat | s\nach | A | counter | s | 1 | - | A | a\n", 2, "bad kind");
    expect_bake_error("stat | s\nach | A | bool | s | 0 | - | A | a\n", 2, "bool binding a stat");
    expect_bake_error("stat | s\nach | A | bool | | 0 | weird | A | a\n", 2, "bad flags");
    expect_bake_error("ach | A | bool | | 0 | - | A\n", 1, "missing column");
    expect_bake_error("stat |\n", 1, "empty stat key");
    expect_bake_error("blah | x\n", 1, "unknown record");
}

void test_conflict_with_code() {
    const std::vector<uint8_t> bytes = bake_ok(SOURCE, "bake for conflict");
    ach::Registry a;
    check(ach::load_manifest(a, bytes.data(), bytes.size()) == ach::LoadResult::Ok, "manifest first");
    check(a.define({"BOSS_DOWN", "x", "y", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Duplicate, "code cannot redefine baked id");
    check(a.define({"PROC_1", "x", "y", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Ok, "procedural definition accepted");

    ach::Registry b;
    check(b.define({"BOSS_DOWN", "x", "y", ach::Kind::Boolean, nullptr, 0, 0}) ==
              ach::DefineResult::Ok, "code first");
    check(ach::load_manifest(b, bytes.data(), bytes.size()) == ach::LoadResult::Duplicate,
          "manifest cannot redefine code id");
}

void test_corruption() {
    const std::vector<uint8_t> good = bake_ok(SOURCE, "bake for corruption");
    if (good.empty()) return;
    ach::Registry probe;
    check(ach::load_manifest(probe, good.data(), 8) == ach::LoadResult::TooShort, "truncated header");
    check(ach::load_manifest(probe, nullptr, 0) == ach::LoadResult::TooShort, "null base");
    check(ach::load_manifest(probe, good.data(), good.size() - 1) == ach::LoadResult::BadLayout,
          "size mismatch");

    int accepted = 0;
    int rejected = 0;
    for (std::size_t i = 0; i < good.size(); ++i) {
        for (int bit = 0; bit < 8; bit += 3) {
            std::vector<uint8_t> bad = good;
            bad[i] ^= static_cast<uint8_t>(1u << bit);
            ach::Registry reg;
            if (ach::load_manifest(reg, bad.data(), bad.size()) == ach::LoadResult::Ok) {
                ++accepted;
                for (const ach::Entry& e : reg.entries()) {
                    check(e.key != nullptr && e.name != nullptr && e.desc != nullptr, "strings set");
                }
            } else {
                ++rejected;
            }
        }
    }
    std::printf("  bit-flips: %d rejected, %d tolerated without crash\n", rejected, accepted);
}

} // namespace

int main() {
    std::printf("achievements manifest seam\n");
    test_seam();
    test_determinism();
    test_source_errors();
    test_conflict_with_code();
    test_corruption();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
