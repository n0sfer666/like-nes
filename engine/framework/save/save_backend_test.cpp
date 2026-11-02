#include "save_backend.hpp"
#include "save_slots.hpp"
#include "save_format.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "platform_args.hpp"
#include "platform_fs.hpp"

namespace {
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-backend: FAIL %s\n", why); } }
bool migrate(std::vector<uint8_t>& body) { body.push_back(8); return true; }
}

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc != 2) { std::puts("usage: save_backend_test ABSOLUTE_TEMP_DIR"); return 1; }
    using save::ReadResult;
    using save::MoveResult;
    const std::string root = std::string(argv[1]) + "/backend-\xd1\x82\xd0\xb5\xd1\x81\xd1\x82";
    save::LocalBackend backend(root);
    check(backend.root() == root && platform::is_dir(root), "absolute UTF-8 root created");
    const std::vector<uint8_t> large(8192, 42), small{0, 255, 3};
    std::vector<uint8_t> got;
    check(backend.read("missing", got, 24) == ReadResult::Missing && got.empty(), "missing distinct from IO failure");
    check(backend.write("progress", large), "durable write");
    check(backend.read("progress", got, 24) == ReadResult::TooLarge && got == std::vector<uint8_t>(24, 42), "bounded header prefix preserved");
    check(backend.read("progress", got, 0) == ReadResult::TooLarge && got.empty(), "zero byte read is bounded");
    check(backend.read("progress", got, large.size()) == ReadResult::Ok && got == large, "exact size is not too large");
    check(backend.write("progress", small) && backend.read("progress", got, 100) == ReadResult::Ok && got == small, "replace and exact bytes");
    check(backend.write("empty", {}) && backend.read("empty", got, 0) == ReadResult::Ok && got.empty(), "empty file at zero limit");
    std::vector<std::string> keys;
    check(backend.list(keys) && std::is_sorted(keys.begin(), keys.end()) &&
          std::find(keys.begin(), keys.end(), "progress") != keys.end(), "stable list");
    for (const std::string key : {"../escape", "a/b", "a\\b", ".", "..", "trailing.", "a b", "nul", "con.dat", "com9.bak", "Progress", "progress.tmp", "progress.TMP"}) {
        check(!backend.write(key, small), "invalid/DOS key rejected before file access");
        got = large; check(backend.read(key, got, 24) == ReadResult::Error && got.empty(), "invalid read clears output");
    }
    check(backend.write("progress.keep", small) && !backend.write("progress.tmp", large), "temp namespace cannot be another logical record");
    check(platform::write_atomic(root + "/progress.tmp", large), "seed physical service temp");
    check(backend.list(keys) && std::find(keys.begin(), keys.end(), "progress.tmp") == keys.end(), "list hides service temp");
    check(backend.write("progress", small) && backend.read("progress.keep", got, 100) == ReadResult::Ok && got == small,
          "writing one logical key preserves its neighbour");
    check(backend.move_new("progress", "progress.tmp") == MoveResult::Error &&
          backend.move_new("progress.tmp", "other") == MoveResult::Error, "move cannot collide with writer temp");
    check(!backend.write(std::string(129, 'a'), small) && !backend.write(std::string("a\0b", 3), small), "long/NUL key rejected");
    check(backend.move_new("progress", "progress.v2.0.pre") == MoveResult::Moved, "exclusive quarantine move");
    check(backend.read("progress", got, 100) == ReadResult::Missing &&
          backend.read("progress.v2.0.pre", got, 100) == ReadResult::Ok && got == small, "new copy intact and original removed");
    check(backend.write("progress", large), "next source written");
    check(backend.move_new("progress", "progress.v2.0.pre") == MoveResult::Exists, "existing backup not overwritten");
    check(backend.read("progress", got, large.size()) == ReadResult::Ok && got == large &&
          backend.read("progress.v2.0.pre", got, 100) == ReadResult::Ok && got == small, "refused move preserves both files");
    check(backend.move_new("progress", "../escape") == MoveResult::Error &&
          backend.move_new("progress", "progress") == MoveResult::Error, "invalid/same move refused");
    check(backend.move_new("missing", "fresh.bad") == MoveResult::Error && !platform::file_exists(root + "/fresh.bad"), "missing source creates no backup");
    check(platform::make_dir(root + "/occupied"), "occupied directory fixture");
    check(backend.move_new("progress", "occupied") == MoveResult::Exists &&
          backend.read("occupied", got, 10) == ReadResult::Error, "directory collision/read cannot erase source");
    check(backend.read("progress", got, large.size()) == ReadResult::Ok && got == large, "all failed moves preserve source");
    save::LocalBackend relative("relative"), absent(""), nul(root + std::string(1, '\0') + "suffix");
    check(relative.root().empty() && absent.root().empty() && nul.root().empty(), "no root fallback");
    check(!relative.write("x", small) && relative.read("x", got, 1) == ReadResult::Error &&
          !relative.list(keys) && keys.empty(), "invalid backend never writes to cwd");
    save::LocalBackend bad(root + "/progress/child");
    check(bad.root().empty() && !bad.write("x", small), "file parent refuses root creation");
    check(save::store(backend, "checkpoint", 1, small), "slot writes through real LocalBackend");
    const std::array<save::Migrate, 1> chain{migrate};
    std::string quarantine;
    check(save::load(backend, "checkpoint", 2, chain, got, &quarantine) == save::Status::Migrated &&
          got == std::vector<uint8_t>({0, 255, 3, 8}) && quarantine == "checkpoint.v1.0.pre", "real migration and backup");
    check(backend.read(quarantine, got, 100) == ReadResult::Ok, "real backup readable");
    save::RecordView record;
    check(save::decode_record(got, record) == save::RecordResult::Ok && record.writes == 1 && record.version == 1,
          "real premigration copy retains old version and counter");
    for (int i = 0; i < 2; ++i) {
        check(save::encode_record(2, 70 + i, small, got), "corruption fixture encoded");
        got.back() ^= 1;
        check(backend.write("checkpoint", got) && save::load(backend, "checkpoint", 2, chain, got, &quarantine) == save::Status::Corrupt &&
              quarantine == "checkpoint." + std::to_string(i) + ".bad", "real repeated quarantine exclusive");
    }
    check(save::store(backend, "checkpoint", 2, small) && backend.read("checkpoint", got, 100) == ReadResult::Ok &&
          save::decode_record(got, record) == save::RecordResult::Ok && record.writes == 72, "real counter survives corrupt quarantine");
    std::puts(fails ? "save-backend: FAIL" : "save-backend: PASS bounded reads, keys, roots, exclusive move and preservation");
    return fails ? 1 : 0;
}
