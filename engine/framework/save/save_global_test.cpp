#include "save.hpp"
#include "save_format.hpp"

#include <array>
#include <cstdio>

#include "platform_args.hpp"
#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "platform_save.hpp"

namespace {
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("save-global: FAIL %s\n", why); } }
}
int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc != 2) return 1;
    using save::Status;
    const std::string room = argv[1], env = room + "/env", old = room + "/old", shell = room + "/shell";
    const std::vector<uint8_t> bytes{1, 0, 255}, other{9, 8};
    std::vector<uint8_t> body; std::string quarantine;
    check(platform::env_put("LIKENES_SAVE_DIR", env.c_str()), "controlled env fixture, including refusal controls");
    check(save::dir().empty() && !save::store("progress", 1, bytes) &&
          !platform::file_exists(room + "/progress"), "uninitialized facade cannot write to cwd");
    check(save::init("test-game") && save::dir() == env, "absolute env override selected");
    check(save::store("progress", 1, bytes) && save::load("progress", 1, {}, body) == Status::Ok && body == bytes,
          "global roundtrip");
    check(save::file_path("controls.txt") == env + "/controls.txt" && save::file_path("../escape").empty() &&
          save::file_path("progress.tmp").empty(), "compatibility path validated");
    check(save::init("test-game") && save::load("progress", 1, {}, body) == Status::Ok && body == bytes,
          "reinit does not erase slot");
    check(platform::ensure_dir(old), "old root created");
    const std::array<std::string, 3> names{"achievements.save", "controls.txt", "controls-p2.txt"};
    for (const auto& name : names) check(platform::write_atomic(old + "/" + name, bytes), "legacy fixture");
    check(platform::write_atomic(old + "/unrelated", other) && platform::write_atomic(env + "/controls.txt", other), "existing destination and neighbour");
    check(save::migrate_legacy(old, names), "legacy files transferred without overwrite");
    for (const std::string name : {"achievements.save", "controls-p2.txt"})
        check(!platform::file_exists(old + "/" + name) && platform::read_bytes(env + "/" + name, body) && body == bytes,
              "legacy bytes unchanged and source removed only after durable copy");
    check(platform::read_bytes(env + "/controls.txt", body) && body == other &&
          platform::read_bytes(old + "/controls.txt", body) && body == bytes &&
          platform::read_bytes(old + "/unrelated", body) && body == other, "existing target preserves both, unrelated file intact");
    check(save::migrate_legacy(old, names) && save::migrate_legacy(env, names), "repeat and same-root migration safe");
    const std::array<std::string, 1> invalid{"../escape"};
    check(!save::migrate_legacy(old, invalid) && !save::migrate_legacy("relative", names), "legacy cannot traverse or use implicit cwd");
    check(save::migrate_legacy(room + "/absent", names) && save::migrate_legacy("", names), "missing source is no-op");
    check(platform::make_dir(old + "/controls-p2.txt") && !save::migrate_legacy(old, names), "directory is not a legacy file");
    std::vector<uint8_t> future;
    check(save::encode_record(9, 10, bytes, future) && platform::write_atomic(env + "/future", future), "future fixture");
    check(save::load("future", 1, {}, body, &quarantine) == Status::Future && body.empty() &&
          quarantine == env + "/future.v9.0.bak" && platform::read_bytes(quarantine, body) && body == future,
          "global quarantine returns absolute path with original bytes");
    check(save::store("future", 1, bytes), "global new slot after future quarantine");
    check(platform::env_put("LIKENES_SAVE_DIR", "relative"), "relative override fixture");
    check(!save::init("test-game") && save::dir().empty() && !save::store("progress", 1, bytes) &&
          !platform::is_dir(room + "/relative"), "bad env refuses OS and cwd fallback");
    check(platform::env_put("LIKENES_SAVE_DIR", env.c_str()) && platform::set_save_dir(shell), "shell priority fixture");
    check(save::init("test-game") && save::dir() == shell && save::store("progress", 1, other), "shell wins over desktop env");
    check(!save::init("../bad-app") && save::dir().empty(), "invalid app rejected");
    check(platform::write_atomic(room + "/occupied", bytes) && platform::set_save_dir(room + "/occupied/child"), "bad parent fixture");
    check(!save::init("test-game") && save::dir().empty() && save::file_path("controls.txt").empty() &&
          !save::store("progress", 1, bytes), "unusable shell dir refuses all fallback");
    check(platform::remove_file(room + "/occupied"), "blocked parent repaired");
    check(save::load("bad.name", 1, {}, body) == Status::Invalid &&
          save::load("progress", 2, {}, body) == Status::Invalid && !save::store("bad.name", 1, bytes) &&
          !platform::is_dir(room + "/occupied/child"), "invalid global call cannot trigger directory recovery IO");
    check(save::store("progress", 1, bytes) &&
          save::dir() == room + "/occupied/child", "next store retries same intended directory after repair");
    check(!platform::set_save_dir("") && !save::init("test-game") && save::dir().empty(), "explicit shell refusal beats env");
    check(save::load("progress", 1, {}, body, &quarantine) == Status::Error && body.empty() && quarantine.empty(), "unavailable load clears outputs");
    check(save::load(nullptr, 1, {}, body) == Status::Invalid && !save::store(nullptr, 1, bytes), "null slot safe");
    check(!platform::file_exists(room + "/progress"), "no cwd write on any failure path");
    std::puts(fails ? "save-global: FAIL" : "save-global: PASS root policy, facade, lossless legacy move and refusal");
    return fails ? 1 : 0;
}
