#include "frame_stats.hpp"

#include <cstdio>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

} // namespace

int main() {
    framework::core::FrameStats stats;
    check(stats.init(8), "cold initialization reserves collector storage");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool plain_ok = framework::probe::control::plain_allocation();
    const long plain_allocs = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(plain_ok && plain_allocs > 0, "control: ordinary allocation is observed");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool aligned_ok = framework::probe::control::aligned_allocation();
    const long aligned_allocs = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(aligned_ok && aligned_allocs > 0, "control: aligned allocation is observed");

    framework::probe::allocs = 0;
    framework::probe::in_hot = true;
    for (uint64_t i = 0; i < 8; ++i) stats.record(i == 0 ? 0 : 16666667, i);
    const framework::core::FrameStatsResult result = stats.summary();
    const long collector_allocs = framework::probe::allocs;
    framework::probe::in_hot = false;

    check(result.complete && result.frames == 8 && result.intervals == 7,
          "hot records and optional summary produce a complete report");
    check(collector_allocs == 0, "record and summary allocate nothing after cold init");
    std::printf("framework-frame-stats-rt: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
