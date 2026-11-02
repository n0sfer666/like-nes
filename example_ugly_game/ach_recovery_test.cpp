#include "achievements.hpp"
#include "assets_path.hpp"
#include "ach_recovery_io.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "ach_bake.hpp"
#include "bundle_writer.hpp"
#include "platform_args.hpp"
#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "platform_save.hpp"
#include "store.hpp"
#include "tracker.hpp"

namespace {
int fails;
const auto KILLS = ach::hash_key("stat_kills"), SCORE = ach::hash_key("stat_score"), DEATHS = ach::hash_key("stat_deaths");
const auto BOSS = ach::hash_key("BOSS_DOWN"), FLAWLESS = ach::hash_key("FLAWLESS"), HIGH = ach::hash_key("SCORE_1000");
const auto UNKNOWN_STAT = ach::hash_key("future_stat"), UNKNOWN_ACH = ach::hash_key("future_ach");
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("ach-recovery: FAIL %s\n", why); } }
uint64_t stat(const ach::Snapshot& snap, ach::Id id) {
    for (const auto& value : snap.stats) if (value.id == id) return value.value;
    ++fails; return 0;
}
bool unlocked(const ach::Snapshot& snap, ach::Id id) { return std::find(snap.unlocked.begin(), snap.unlocked.end(), id) != snap.unlocked.end(); }
ach::Snapshot read(const std::string& path) {
    ach::Snapshot snap;
    check(ach::LocalStore(path).read(snap), "actual saved LN contents decode");
    return snap;
}
std::vector<uint8_t> encode(const ach::Snapshot& snapshot) { std::vector<uint8_t> bytes; ach::encode(snapshot, bytes); return bytes; }
bool exact(const std::string& path, const std::vector<uint8_t>& expected) {
    std::vector<uint8_t> bytes;
    return platform::read_bytes(path, bytes) && bytes == expected;
}
void observe(game::Achievements& service) {
    game::GameState gs;
    gs.kills = 3; gs.score = 200; gs.lives = 1; gs.phase = game::PH_Victory;
    service.observe(gs); // session: 3 kills / 200 score / 2 deaths + BOSS_DOWN.
}
bool root(const std::string& path) {
    check(platform::env_put("LIKENES_SAVE_DIR", path.c_str()) && platform::set_save_dir(path), "isolated root policy");
    return game::init_save_paths();
}
std::string bundle(const std::string& room) {
    std::vector<uint8_t> table; ach::BakeError error;
    check(ach::bake_manifest_file(ACH_MANIFEST, table, error), "bake real game catalogue");
    asset::AssetInput asset{};
    asset.guid = asset::fnv1a("achievements", 12); asset.type = asset::AssetType::Raw;
    asset.codec = asset::Codec::Raw; asset.residency = asset::Residency::Mmap;
    asset.uncompressed_size = static_cast<uint32_t>(table.size()); asset.payload = std::move(table);
    const std::string path = room + "/catalogue.bundle";
    check(platform::write_atomic(path, asset::write_bundle({std::move(asset)})), "publish real bundle fixture");
    return path;
}
}

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc != 2) return 1;
    const std::string room = argv[1], source = room + "/old", blocked = room + "/blocked", destination = blocked + "/child";
    const auto catalogue = bundle(room);
    const ach::Snapshot original{{{KILLS, 10}, {SCORE, 950}, {DEATHS, 5}, {UNKNOWN_STAT, 77}}, {FLAWLESS, UNKNOWN_ACH}};
    const auto original_bytes = encode(original);
    check(platform::ensure_dir(source) && platform::write_atomic(source + "/achievements.save", original_bytes) &&
        platform::write_atomic(blocked, {original_bytes.data(), 1}), "old snapshot and blocked destination");
    ach::Snapshot blocked_snapshot; ach::DecodeResult blocked_reason;
    check(!ach::LocalStore(destination + "/achievements.save").read(blocked_snapshot, &blocked_reason) &&
          blocked_reason == ach::DecodeResult::Unreadable, "bad parent is IO failure, not missing snapshot");
    check(platform::env_put("LIKENES_SAVE_DIR", source.c_str()) && platform::set_save_dir(destination) && !game::init_save_paths(),
        "initial mobile-like destination failure, separate legacy env retained");
    game::Achievements live;
    live.init(catalogue, game::achievement_save_path(), "", game::achievement_save_path);
    observe(live); live.save();
    check(exact(source + "/achievements.save", original_bytes), "failed initial save never touches legacy original");
    check(platform::remove_file(blocked) && platform::ensure_dir(destination), "repair destination");
    const std::vector<uint8_t> occupied{9, 8};
    check(platform::write_atomic(destination + "/achievements.save.0.pre", occupied), "existing backup collision");
    live.save();
    const std::string path = destination + "/achievements.save";
    auto snap = read(path);
    check(stat(snap, KILLS) == 13 && stat(snap, SCORE) == 1150 && stat(snap, DEATHS) == 7, "old lifetime + live session deltas");
    check(unlocked(snap, BOSS) && unlocked(snap, FLAWLESS) && unlocked(snap, HIGH) && unlocked(snap, UNKNOWN_ACH) &&
        stat(snap, UNKNOWN_STAT) == 77, "union, retroactive threshold and unknown records retained");
    check(!platform::file_exists(source + "/achievements.save") && exact(destination + "/achievements.save.1.pre", original_bytes) &&
        exact(destination + "/achievements.save.0.pre", occupied), "legacy transferred, original bytes backed up exclusively");
    live.save(); snap = read(path);
    check(stat(snap, KILLS) == 13 && stat(snap, SCORE) == 1150 && !platform::file_exists(destination + "/achievements.save.2.pre"), "repeat never re-adds old stats");
    game::GameState next;
    next.kills = 5; next.score = 300; next.lives = 1;
    live.observe(next); live.save(); snap = read(path);
    check(stat(snap, KILLS) == 15 && stat(snap, SCORE) == 1250 && stat(snap, DEATHS) == 7, "new events after merge keep lifetime totals");
    game::Achievements restarted;
    restarted.init(catalogue, path, "", game::achievement_save_path); restarted.save();
    check(stat(read(path), KILLS) == 15 && restarted.unlocked_count() == live.unlocked_count(), "normal restart loads without late sum");

    // Initially and late discovered bad/future/over-carry images are never replaced with defaults.
    for (int mode = 0; mode < 4; ++mode) {
        const std::string where = room + "/reject-" + std::to_string(mode);
        check(root(where), "rejection root");
        auto bytes = original_bytes;
        if (mode == 0) bytes.back() ^= 1;
        if (mode == 1) bytes[4] = 2;
        if (mode == 2) bytes.clear();
        if (mode == 3) {
            ach::Snapshot too_many;
            for (size_t i = 0; i <= ach::Tracker::MAX_CARRIED; ++i) too_many.stats.push_back({i, 1});
            bytes = encode(too_many);
        }
        const std::string rejected = where + "/achievements.save";
        check(platform::write_atomic(rejected, bytes), "invalid original fixture");
        game::Achievements service;
        service.init(catalogue, game::achievement_save_path(), "", game::achievement_save_path);
        observe(service); service.save(); service.save();
        check(exact(rejected, bytes) && !platform::file_exists(rejected + ".0.pre"), "unsupported original neither moved nor overwritten");
        check(platform::write_atomic(rejected, original_bytes), "operator repairs invalid image");
        service.save();
        check(stat(read(rejected), KILLS) == 13 && exact(rejected + ".0.pre", original_bytes), "retry after format repair preserves accumulated session");
    }
    for (int mode = 0; mode < 2; ++mode) {
        const std::string blocker = room + "/partial-" + std::to_string(mode), where = blocker + "/child";
        check(platform::write_atomic(blocker, occupied) && !root(where), "partial old initial failure");
        game::Achievements partial;
        partial.init(catalogue, game::achievement_save_path(), "", game::achievement_save_path); observe(partial);
        check(platform::remove_file(blocker) && platform::ensure_dir(where), "partial old repair");
        ach::Snapshot old;
        if (mode == 0) old.stats = {{KILLS, 10}};
        old.unlocked = {FLAWLESS};
        check(platform::write_atomic(where + "/achievements.save", encode(old)), "partial/empty old stats image");
        partial.save();
        const auto merged = read(where + "/achievements.save");
        check(stat(merged, KILLS) == (mode == 0 ? 13u : 3u) && stat(merged, SCORE) == 200 && stat(merged, DEATHS) == 2 &&
              unlocked(merged, BOSS) && unlocked(merged, FLAWLESS), "missing old IDs contribute zero, session never doubled");
    }
    for (int mode = 0; mode < 2; ++mode) {
        const std::string blocker = room + "/publish-" + std::to_string(mode), where = blocker + "/child";
        check(platform::write_atomic(blocker, occupied) && !root(where), "publication initial failure");
        game::Achievements pending;
        pending.init(catalogue, game::achievement_save_path(), "", game::achievement_save_path); observe(pending);
        check(platform::remove_file(blocker) && platform::ensure_dir(where), "publication root repaired");
        const std::string current = where + "/achievements.save";
        check(platform::write_atomic(current, original_bytes), "publication old image");
        const int before = game::test::injected_failures();
        game::test::fail_next_write_to(current, mode == 1);
        pending.save();
        check(game::test::injected_failures() == before + 1 && exact(current + ".0.pre", original_bytes), "real write fault reached after durable backup");
        check(platform::file_exists(current) == (mode == 1), "false before publish leaves backup only, false after leaves complete current");
        if (mode == 1) check(stat(read(current), KILLS) == 13, "new complete image visible despite false result");
        pending.save();
        const auto merged = read(current);
        check(stat(merged, KILLS) == 13 && stat(merged, SCORE) == 1150 && stat(merged, DEATHS) == 7 &&
              !platform::file_exists(current + ".1.pre"), "retry after failed publication never merges own image twice");
    }
    const std::string overflow_block = room + "/overflow", overflow = overflow_block + "/child";
    check(platform::write_atomic(overflow_block, occupied) && !root(overflow), "overflow initial failure");
    game::Achievements saturated;
    saturated.init(catalogue, game::achievement_save_path(), "", game::achievement_save_path); observe(saturated);
    check(platform::remove_file(overflow_block) && platform::ensure_dir(overflow), "overflow root repair");
    auto almost_max = original;
    almost_max.stats[1].value = std::numeric_limits<uint64_t>::max() - 2;
    check(platform::write_atomic(overflow + "/achievements.save", encode(almost_max)), "overflow old score");
    saturated.save();
    check(stat(read(overflow + "/achievements.save"), SCORE) == std::numeric_limits<uint64_t>::max(), "sum saturates, never wraps");
    std::puts(fails ? "ach-recovery: FAIL" : "ach-recovery: PASS real consumer late merge, original backups, retry, unknown refusal and saturation");
    return fails ? 1 : 0;
}
