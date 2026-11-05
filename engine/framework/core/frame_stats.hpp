#pragma once

#include <cstdint>
#include <vector>

namespace framework::core {

struct FrameStatsResult {
    uint64_t frames = 0;
    uint64_t intervals = 0;
    uint64_t misses = 0;
    uint64_t work_p50_ns = 0;
    uint64_t work_p99_ns = 0;
    uint64_t interval_p50_ns = 0;
    uint64_t interval_p99_ns = 0;
    bool complete = false;
};

// Single-owner, immobile collector. Reserve in place during init; record() performs no allocations.
class FrameStats {
public:
    FrameStats() = default;
    FrameStats(const FrameStats&) = delete;
    FrameStats& operator=(const FrameStats&) = delete;
    FrameStats(FrameStats&&) = delete;
    FrameStats& operator=(FrameStats&&) = delete;

    // Capacity must be in [1, 1,000,000]. An invalid capacity leaves the current collector intact.
    bool init(uint32_t capacity);
    // Interval0 omits cadence after startup/resume; work0 remains a valid work sample.
    // Capacity exhaustion refuses the record and makes the capture incomplete until init.
    bool record(uint64_t interval_ns, uint64_t work_ns);
    // Sorts samples in place outside the measured frame. Later records are still permitted.
    FrameStatsResult summary();
    uint32_t size() const;

private:
    std::vector<uint64_t> work_samples_;
    std::vector<uint64_t> interval_samples_;
    uint64_t frames_ = 0;
    uint64_t misses_ = 0;
    uint32_t capacity_ = 0;
    bool initialized_ = false;
    bool valid_ = false;
};

} // namespace framework::core
