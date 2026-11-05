#include "rumble_frame_stats.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_save.hpp"
#include "save_slots.hpp"

namespace {
using namespace std::chrono_literals;
using rumble::FrameClock;
using rumble::FrameStatsRun;

bool check_saved(const std::string& root) {
    const std::string base = root + "/like-nes-frame-stats";
    std::vector<std::string> entries;
    if (!platform::list_dir(base, entries) || entries.size() != 1) return false;
    save::LocalBackend backend(base + "/" + entries.front());
    std::vector<uint8_t> body;
    return save::load(backend, "sample", 1, {}, body) == save::Status::Ok &&
        std::string(body.begin(), body.end()) == "frames=4 intervals=2 misses=1\n";
}

void exercise(FrameStatsRun& run, std::string_view mode) {
    auto begin = FrameClock::now();
    for (uint32_t i = 0; i < FrameStatsRun::WARMUP_FRAMES; ++i) {
        if (mode != "silent") run.tick();
        run.observe_touch(true, true); // Must be ignored during warmup.
        run.present(begin, begin + 1ms, true);
        begin += 16666667ns;
    }
    // Real callback and worker, synthetic render timestamps. No GPU claim.
    std::this_thread::sleep_for(600ms);
    run.poll();
    run.present(begin, begin + 1ms, true);
    begin += 33333333ns; // One render/acquire failed: no record between these starts.
    run.present(begin, begin + 2ms, true);
    run.suspend();
    run.resume();
    begin += 5s;
    if (mode != "no-touch") run.observe_touch(true, true);
    run.present(begin, begin + 3ms, false);
    begin += 16666667ns;
    run.present(begin, begin + 4ms, false);
    run.present(begin + 20ms, begin + 29ms, false); // Bounded; must not change results.
}
} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc != 3 || !platform::set_save_dir(argv[2])) return 2;
    const std::string_view mode = argv[1];
    FrameStatsRun run;
    std::string why;
    const auto bundle = platform::exe_dir() + "/frame-stats.bundle";
    if (mode == "bad-bundle")
        return !run.open(bundle + ".missing", rumble::FrameStatsSource::Fixture, why, 4) &&
            why == "cannot load the benchmark music bundle" ? 0 : 1;
    if (!run.open(bundle, rumble::FrameStatsSource::Fixture, why, 4)) {
        std::fprintf(stderr, "frame-stats-fixture: %s\n", why.c_str());
        return 1;
    }
    exercise(run, mode);
    const bool finished = run.finish(mode != "no-save");
    if (!run.ready() || finished != (mode == "ok") || run.finish(true) != finished) return 1;
    if (mode == "ok" || mode == "no-touch" || mode == "silent") return check_saved(argv[2]) ? 0 : 1;
    return 0;
}
