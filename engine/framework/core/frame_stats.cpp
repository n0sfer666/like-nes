#include "frame_stats.hpp"

#include <algorithm>
#include <new>

namespace framework::core {
namespace {

constexpr uint64_t NANOSECONDS_PER_SECOND = 1000000000ULL;
constexpr uint64_t FRAMES_PER_SECOND = 60;

uint64_t nearest_periods(uint64_t interval_ns) {
    const uint64_t whole_seconds = interval_ns / NANOSECONDS_PER_SECOND;
    const uint64_t remainder_ns = interval_ns % NANOSECONDS_PER_SECOND;
    const uint64_t whole_periods = whole_seconds * FRAMES_PER_SECOND;
    const uint64_t fractional_periods =
        (remainder_ns * FRAMES_PER_SECOND + NANOSECONDS_PER_SECOND / 2) /
        NANOSECONDS_PER_SECOND;
    return whole_periods + fractional_periods;
}

uint64_t percentile(const std::vector<uint64_t>& samples, uint32_t numerator) {
    if (samples.empty()) return 0;
    const std::size_t index = (static_cast<std::size_t>(numerator) * samples.size()) / 100;
    return samples[std::min(index, samples.size() - 1)];
}

} // namespace

bool FrameStats::init(uint32_t capacity) {
    if (capacity == 0 || capacity > 1000000) return false;

    std::vector<uint64_t> work;
    std::vector<uint64_t> intervals;
    try {
        work.reserve(capacity);
        intervals.reserve(capacity);
    } catch (const std::bad_alloc&) {
        return false;
    }

    work_samples_.swap(work);
    interval_samples_.swap(intervals);
    frames_ = 0;
    misses_ = 0;
    capacity_ = capacity;
    initialized_ = true;
    valid_ = true;
    return true;
}

bool FrameStats::record(uint64_t interval_ns, uint64_t work_ns) {
    if (!initialized_) return false;
    if (!valid_) return false;
    if (frames_ >= capacity_) {
        valid_ = false;
        return false;
    }

    uint64_t added_misses = 0;
    if (interval_ns != 0) {
        const uint64_t periods = nearest_periods(interval_ns);
        if (periods > 1) added_misses = periods - 1;
        if (misses_ > UINT64_MAX - added_misses) {
            valid_ = false;
            return false;
        }
    }

    work_samples_.push_back(work_ns);
    if (interval_ns != 0) interval_samples_.push_back(interval_ns);
    misses_ += added_misses;
    ++frames_;
    return true;
}

FrameStatsResult FrameStats::summary() {
    std::sort(work_samples_.begin(), work_samples_.end());
    std::sort(interval_samples_.begin(), interval_samples_.end());

    FrameStatsResult result;
    result.frames = frames_;
    result.intervals = interval_samples_.size();
    result.misses = misses_;
    result.work_p50_ns = percentile(work_samples_, 50);
    result.work_p99_ns = percentile(work_samples_, 99);
    result.interval_p50_ns = percentile(interval_samples_, 50);
    result.interval_p99_ns = percentile(interval_samples_, 99);
    result.complete = initialized_ && valid_ && frames_ != 0 &&
                      !work_samples_.empty() && !interval_samples_.empty();
    return result;
}

uint32_t FrameStats::size() const {
    return static_cast<uint32_t>(work_samples_.size());
}

} // namespace framework::core
