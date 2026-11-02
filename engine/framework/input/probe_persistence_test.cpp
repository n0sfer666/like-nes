#include "probe_persistence.hpp"
#include "rebind_store.hpp"
#include "save.hpp"

#include <cstdio>

#include "platform_args.hpp"
#include "platform_env.hpp"
#include "platform_fs.hpp"

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc != 2) return 1;
    int fails = 0;
    auto check = [&](bool good, const char* why) { if (!good) { ++fails; std::printf("probe-persist: FAIL %s\n", why); } };
    const std::string room = argv[1], blocker = room + "/blocked", root = blocker + "/child";
    const std::vector<uint8_t> bad_parent{1};
    check(platform::write_atomic(blocker, bad_parent) && platform::env_put("LIKENES_SAVE_DIR", root.c_str()), "blocked root fixture");
    check(!save::init("probe-test"), "initial refusal");
    framework::input::RebindStore current;
    current.set("fire", 0, {input::SourceKind::Key, 65});
    bool loaded = false; std::string path;
    check(!framework::input::save_probe_profile(current, "probe", loaded, path) && path.empty() && !loaded, "failed save stays retryable");
    check(platform::remove_file(blocker) && platform::ensure_dir(root), "repair root");
    framework::input::RebindStore old;
    old.set("fire", 0, {input::SourceKind::Key, 66});
    check(old.save(root + "/controls_probe.txt", "probe"), "late profile exists after repair");
    check(platform::write_atomic(root + "/controls_probe.txt.0.pre", bad_parent), "occupied backup fixture");
    check(framework::input::save_probe_profile(current, "probe", loaded, path) && loaded && path == root + "/controls_probe.txt",
          "explicit S saves current bindings after recovery");
    std::string text;
    check(platform::read_text(root + "/controls_probe.txt.1.pre", text) && text == old.serialize("probe"), "late old profile retained byte-for-byte");
    std::vector<uint8_t> got;
    check(platform::read_bytes(root + "/controls_probe.txt.0.pre", got) && got == bad_parent, "existing backup never overwritten");
    check(platform::read_text(path, text) && text == current.serialize("probe"), "current profile written in unchanged format");
    check(framework::input::save_probe_profile(current, "probe", loaded, path) &&
          !platform::file_exists(root + "/controls_probe.txt.2.pre"), "subsequent explicit save does not create redundant backup");
    std::puts(fails ? "probe-persist: FAIL" : "probe-persist: PASS recovery, late old profile preserved and current explicit save");
    return fails ? 1 : 0;
}
