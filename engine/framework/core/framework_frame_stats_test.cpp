#include "frame_stats.hpp"

#include <cstdio>
#include <limits>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<framework::core::FrameStats>);
static_assert(!std::is_copy_assignable_v<framework::core::FrameStats>);
static_assert(!std::is_move_constructible_v<framework::core::FrameStats>);
static_assert(!std::is_move_assignable_v<framework::core::FrameStats>);

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void test_boundaries() {
    framework::core::FrameStats stats;
    check(stats.init(11), "boundary collector initializes");
    constexpr uint64_t intervals[] = {0,        16666666, 16666667, 24999999,
                                      25000000, 33333333, 33333334, 41666666,
                                      41666667, 50000000, 1000000000};
    constexpr uint64_t work[] = {0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    constexpr uint64_t misses[] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 59};
    for (uint32_t i = 0; i < 11; ++i) {
        framework::core::FrameStats single;
        check(single.init(1) && single.record(intervals[i], work[i]), "isolated boundary records");
        check(single.summary().misses == misses[i], "each boundary has its independent expected count");
        check(stats.record(intervals[i], work[i]), "boundary sample is accepted");
    }
    const framework::core::FrameStatsResult result = stats.summary();
    check(result.frames == 11 && result.intervals == 10, "zero interval records work but no cadence");
    check(result.misses == 67, "60 Hz half-up boundaries have the expected miss count");
    check(result.work_p50_ns == 15 && result.work_p99_ns == 20, "work quantiles use upper indexes");
    check(result.interval_p50_ns == 33333334 && result.interval_p99_ns == 1000000000,
          "interval quantiles exclude the zero marker");
    check(result.complete, "nonempty work and interval samples complete the report");
}

void test_quantiles_and_append_after_summary() {
    framework::core::FrameStats stats;
    check(stats.init(101), "quantile collector initializes");
    for (uint64_t i = 0; i < 100; ++i) {
        check(stats.record((100 - i) * 100, 100 - i), "quantile sample is accepted");
    }
    framework::core::FrameStatsResult result = stats.summary();
    check(result.work_p50_ns == 51 && result.work_p99_ns == 100,
          "100 sample work indexes are N/2 and floor(99N/100)");
    check(result.interval_p50_ns == 5100 && result.interval_p99_ns == 10000,
          "100 sample interval indexes are N/2 and floor(99N/100)");
    check(stats.record(10100, 100), "record remains available after an in-place summary sort");
    result = stats.summary();
    check(result.frames == 101 && result.work_p50_ns == 51 && result.work_p99_ns == 100,
          "appending after summary preserves quantiles");
}

void test_empty_zero_and_reset() {
    framework::core::FrameStats stats;
    check(!stats.record(1, 1), "uninitialized collector refuses record");
    check(!stats.summary().complete, "uninitialized report is incomplete");
    check(stats.init(2), "collector initializes");
    framework::core::FrameStatsResult result = stats.summary();
    check(!result.complete && result.frames == 0 && result.intervals == 0,
          "empty report is incomplete");
    check(stats.record(16666667, 0), "zero work duration is valid");
    result = stats.summary();
    check(result.complete && result.work_p50_ns == 0 && result.work_p99_ns == 0,
          "zero work value is retained as a sample");
    check(stats.record(0, 7), "zero interval is accepted as a cadence marker");
    result = stats.summary();
    check(result.frames == 2 && result.intervals == 1 && result.complete,
          "marker increases frames without increasing interval sample count");
    check(!stats.record(16666667, 8), "capacity overrun is refused");
    check(!stats.summary().complete, "capacity overrun permanently invalidates current report");
    check(!stats.init(0) && !stats.init(1000001), "out of range capacities are refused");
    result = stats.summary();
    check(!result.complete && result.frames == 2, "invalid init preserves previous collector state");
    check(stats.init(1), "valid init resets collector");
    result = stats.summary();
    check(result.frames == 0 && result.intervals == 0 && result.misses == 0 && !result.complete,
          "valid init resets counters and samples");
    check(stats.size() == 0, "size reports accepted frame count after reset");
    check(stats.record(0, 9) && !stats.summary().complete,
          "a work sample without cadence still has an incomplete report");
    check(stats.init(2) && stats.record(33333333, 3), "new measured capture starts");
    check(!stats.init(0) && !stats.init(1000001), "invalid reinit is refused during capture");
    result = stats.summary();
    check(result.complete && result.frames == 1 && result.misses == 1,
          "invalid reinit preserves a valid capture and its counters");
}

void test_uint64_maximum() {
    framework::core::FrameStats stats;
    check(stats.init(1), "maximum value collector initializes");
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    check(stats.record(maximum, maximum), "UINT64_MAX interval and work are accepted safely");
    const framework::core::FrameStatsResult result = stats.summary();
    check(result.misses == 1106804644422ULL, "UINT64_MAX cadence arithmetic avoids overflow");
    check(result.work_p50_ns == maximum && result.work_p99_ns == maximum,
          "UINT64_MAX work sample is preserved");
    check(result.interval_p50_ns == maximum && result.interval_p99_ns == maximum,
          "UINT64_MAX interval sample is preserved");
    check(stats.init(2) && stats.record(maximum, 0) && stats.record(maximum, 0),
          "multiple maximum durations are accepted safely");
    check(stats.summary().misses == 2213609288844ULL, "large miss accumulation remains uint64 exact");
    check(stats.init(1000000) && stats.record(1, 0), "maximum supported cold capacity is accepted");
}

} // namespace

int main() {
    test_boundaries();
    test_quantiles_and_append_after_summary();
    test_empty_zero_and_reset();
    test_uint64_maximum();
    std::printf("framework-frame-stats: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
