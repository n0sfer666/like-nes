#include "audio_stereo_ring.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

namespace {
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("audio-stereo-ring: FAIL %s\n", why); } }
audio::StereoFrame sample(uint32_t i) {
    return {static_cast<int16_t>(static_cast<int32_t>(i % 251) - 125), static_cast<int16_t>(static_cast<int32_t>(i % 241) - 120)};
}
bool exact(audio::StereoFrame a, audio::StereoFrame b) { return a.left == b.left && a.right == b.right; }
}
int main() {
    using namespace audio;
    auto ring = std::make_unique<StereoRing>();
    check(StereoRing::CAPACITY >= SAMPLE_RATE / 4, "at least250ms stereo frames");
    StereoFrame out{1, 2};
    check(!ring->take(1, out) && exact(out, {}), "unprimed ring emits silence");
    std::vector<StereoFrame> frames(StereoRing::CAPACITY + 1);
    for (uint32_t i = 0; i < frames.size(); ++i) frames[i] = sample(i);
    check(ring->push(0, frames) == 0, "zero generation refused");
    check(ring->push(UINT64_MAX, frames) == StereoRing::CAPACITY && ring->push(UINT64_MAX, frames) == 0, "capacity counts stereo frames and full ring refuses");
    check(!ring->take(0, out) && exact(out, {}), "invalid generation silences without consuming");
    for (uint32_t i = 0; i < StereoRing::CAPACITY; ++i)
        check(ring->take(UINT64_MAX, out) && exact(out, sample(i)), "FIFO preserves independent channels and64bit tag");
    check(!ring->take(UINT64_MAX, out) && exact(out, {}), "exact EOF silence");
    for (uint32_t round = 0; round < 7; ++round) {
        check(ring->push(1, frames) == StereoRing::CAPACITY, "refill after cursor wraparound of physical buffer");
        check(!ring->take((uint64_t{1} << 32) | 1, out) && exact(out, {}), "all stale generation frames dropped without playing");
        const auto marker = sample(round + 777);
        check(ring->push((uint64_t{1} << 32) | 1, {&marker, 1}) == 1 &&
            ring->take((uint64_t{1} << 32) | 1, out) && exact(out, marker), "new generation after full stale ring retains exact start");
    }
    const std::array<StereoFrame, 3> three{{{10, -20}, {30, -40}, {50, -60}}};
    check(ring->push(3, three) == 3 && ring->push(4, three) == 3, "stale/new share one queue");
    check(ring->take(4, out) && exact(out, three[0]), "skip stale then take first matching frame");
    check(ring->take(4, out) && exact(out, three[1]) && ring->take(4, out) && exact(out, three[2]), "remaining new generation order");
    check(ring->push(9, std::span(frames).first(StereoRing::CAPACITY - 2)) == StereoRing::CAPACITY - 2 &&
        ring->push(9, three) == 2, "partial push reports exact accepted frame count");
    for (uint32_t i = 0; i < StereoRing::CAPACITY - 2; ++i) check(ring->take(9, out) && exact(out, sample(i)), "partial push preserves existing samples");
    check(ring->take(9, out) && exact(out, three[0]) && ring->take(9, out) && exact(out, three[1]) && !ring->take(9, out), "partial tail never duplicates refused sample");
    framework::probe::in_hot = true;
    const bool controls = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(controls && framework::probe::allocs >= 2, "plain/aligned allocator positive controls");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    for (uint32_t i = 0; i < 1000; ++i) {
        ring->push(11, three); ring->take(12, out); ring->take(12, out); ring->push(12, three); ring->take(12, out);
    }
    framework::probe::in_hot = false;
    check(framework::probe::allocs == 0, "push, stale drain, refill and underrun allocate nothing");
    std::puts(fails ? "audio-stereo-ring: FAIL" : "audio-stereo-ring: PASS stereo FIFO, bounds, generations, partial writes and zero allocations");
    return fails ? 1 : 0;
}
