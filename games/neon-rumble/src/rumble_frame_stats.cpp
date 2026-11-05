#include "rumble_frame_stats.hpp"

#include <atomic>
#include <cstdio>
#include <vector>

#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "platform_path.hpp"
#include "platform_process.hpp"
#include "platform_save.hpp"
#include "save_slots.hpp"

namespace rumble {
namespace {

using framework::core::FrameStatsResult;

uint64_t nanoseconds(FrameClock::duration d) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
}

unsigned long long number(uint64_t n) { return static_cast<unsigned long long>(n); }
const char* flag(bool b) { return b ? "true" : "false"; }

const char* provenance(FrameStatsSource source) {
    switch (source) {
        case FrameStatsSource::Desktop: return "desktop-scripted-touch";
        case FrameStatsSource::Mobile: return "mobile-os-touch";
        case FrameStatsSource::Fixture: return "fixture-null-audio";
    }
    return "unknown";
}

struct SaveSample { uint64_t ns = 0; bool ok = false; };

SaveSample write_sample(const FrameStatsResult& result) {
    const auto* override = platform::save_dir_override();
    std::string env;
    if (!override && platform::env_var("LIKENES_SAVE_DIR", env) && !env.empty()) override = &env;
    if (override && !platform::is_absolute(*override)) return {};
    const std::string base = override ? *override + "/like-nes-frame-stats"
                                      : platform::user_data_dir("like-nes-frame-stats");
    if (base.empty()) return {};
    static std::atomic<uint32_t> sequence{0};
    const std::string dir = base + "/run-" + std::to_string(platform::process_id()) + "-" +
        std::to_string(FrameClock::now().time_since_epoch().count()) + "-" +
        std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    save::LocalBackend backend(dir);
    // Refuse an unexpectedly reused directory. Never load/migrate/quarantine existing records.
    std::vector<std::string> keys;
    if (backend.root().empty() || !platform::list_dir(dir, keys) || !keys.empty()) return {};
    char data[96];
    const int length = std::snprintf(data, sizeof data, "frames=%llu intervals=%llu misses=%llu\n",
        number(result.frames), number(result.intervals), number(result.misses));
    if (length < 0 || static_cast<size_t>(length) >= sizeof data) return {};
    const auto begin = FrameClock::now();
    const bool ok = save::store(backend, "sample", 1,
        {reinterpret_cast<const uint8_t*>(data), static_cast<size_t>(length)});
    return {nanoseconds(FrameClock::now() - begin), ok};
}

} // namespace

bool FrameStatsRun::open(const std::string& bundle, FrameStatsSource source, std::string& why,
                         uint32_t capture_frames) {
    if (opened_ || !frames_.init(capture_frames)) {
        why = "cannot initialize the benchmark frame collector";
        return false;
    }
    if (!music_.open(bundle, source == FrameStatsSource::Fixture, why)) return false;
    source_ = source;
    capacity_ = capture_frames;
    opened_ = true;
    return true;
}

void FrameStatsRun::present(FrameClock::time_point begin, FrameClock::time_point end, bool menu) {
    if (!collecting()) return;
    if (end < begin || (previous_ && begin <= *previous_)) { valid_ = false; return; }
    if (presented_ < WARMUP_FRAMES) {
        if (++presented_ == WARMUP_FRAMES) audio_start_ = music_.stats();
        return;
    }
    if (!frames_.record(previous_ ? nanoseconds(begin - *previous_) : 0, nanoseconds(end - begin))) {
        valid_ = false;
        return;
    }
    previous_ = begin;
    ++presented_;
    if (menu) ++menu_frames_;
    if (ready()) audio_end_ = music_.stats();
}

void FrameStatsRun::observe_touch(bool move, bool attack) {
    if (!collecting() || presented_ < WARMUP_FRAMES) return;
    moved_ |= move;
    attacked_ |= attack;
}

void FrameStatsRun::tick() { if (collecting()) music_.tick(); }
void FrameStatsRun::poll() { if (collecting()) music_.poll(); }
void FrameStatsRun::suspend() {
    reset_cadence();
    paused_ = true;
    music_.suspend();
}

void FrameStatsRun::resume() {
    reset_cadence();
    paused_ = false;
    if (collecting()) music_.resume();
}

bool FrameStatsRun::finish(bool allow_save) {
    if (!opened_ || reported_) return complete_;
    reported_ = true;
    const auto result = frames_.summary();
    const auto audio = ready() ? audio_end_ : music_.stats();
    music_.suspend();
    const auto save = allow_save && ready() ? write_sample(result) : SaveSample{};
    const auto audible = audio.audible_callbacks - audio_start_.audible_callbacks;
    const auto underruns = audio.underruns - audio_start_.underruns;
    complete_ = ready() && valid_ && result.complete && audio_start_.valid && audio.valid && audible != 0 &&
        menu_frames_ != 0 && menu_frames_ < result.frames && moved_ && attacked_ && save.ok;
    std::fprintf(stderr,
        "[frame-stats] {\"schema\":1,\"frames\":%llu,\"intervals\":%llu,\"misses\":%llu,"
        "\"work_p50_ns\":%llu,\"work_p99_ns\":%llu,\"interval_p50_ns\":%llu,\"interval_p99_ns\":%llu,"
        "\"underruns\":%llu,\"save_ms\":%llu.%06llu,\"save_ok\":%s,\"complete\":%s,"
        "\"source\":\"%s\",\"menu_frames\":%u,\"touch_move\":%s,\"touch_attack\":%s,"
        "\"audible_callbacks\":%llu,\"audio_ok\":%s,\"capture_ready\":%s}\n",
        number(result.frames), number(result.intervals), number(result.misses),
        number(result.work_p50_ns), number(result.work_p99_ns), number(result.interval_p50_ns),
        number(result.interval_p99_ns), number(underruns), number(save.ns / 1000000),
        number(save.ns % 1000000), flag(save.ok), flag(complete_), provenance(source_), menu_frames_,
        flag(moved_), flag(attacked_), number(audible), flag(audio_start_.valid && audio.valid), flag(ready()));
    return complete_;
}

} // namespace rumble
