#include "rumble_mobile.hpp"

#include <cstdio>
#include <utility>

#include "mobile_host.hpp"

namespace rumble {

bool MobileRumble::open_benchmark(bool waves) {
    if (waves || mobile::has_arg("--demo")) {
        std::fprintf(stderr, "neon-rumble: --frame-stats needs the standard level and OS touch\n");
        return false;
    }
    auto bench = std::make_unique<FrameStatsRun>();
    std::string why;
    if (!bench->open(mobile::asset_path("frame-stats.bundle"), FrameStatsSource::Mobile, why)) {
        std::fprintf(stderr, "neon-rumble: %s\n", why.c_str());
        return false;
    }
    bench_ = std::move(bench);
    return true;
}

void MobileRumble::suspend() {
    cancel();
    lag_ = 0;
    if (bench_) bench_->suspend();
}

void MobileRumble::resume() {
    lag_ = 0;
    if (bench_) bench_->resume();
}

void MobileRumble::save() {
    // Settings and progress belong to pilot #29. Only a completed benchmark writes its own slot.
    if (bench_ && bench_->ready()) bench_->finish(true);
}

void MobileRumble::shutdown() {
    if (!inited_) return;
    if (bench_) {
        bench_->finish(false);
        bench_.reset();
    }
    cancel();
    inited_ = false;
    pads_ = nullptr;
    release_targets();
    surface_ = nullptr;
}

} // namespace rumble
