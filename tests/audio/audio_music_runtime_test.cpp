#include "music_runtime_fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>

namespace {
int fails;
void check(bool ok, const char* name) {
    if (!ok) { ++fails; std::printf("audio-music-runtime: FAIL %s\n", name); }
}
void transitions() {
    using namespace audio;
    auto owner = music_runtime_fixture::make_rig(); auto& rig = *owner;
    check(rig.ready, "manual MusicWorker initialization over PCM");
    if (!rig.ready) return;
    const MusicMix silent = rig.step(true, {0, 0});
    check(silent.left == 0 && silent.right == 0 && !silent.underruns, "initial priming is silence without underrun");
    const MusicMix instant = rig.step();
    check(instant.left == int64_t{100} * fix32::ONE && instant.right == int64_t{-200} * fix32::ONE,
          "zero fade starts the exact cached intro sample");
    for (uint32_t i = 0; i < 12; ++i) rig.step();
    check(rig.runtime.post({1, 4}), "B wish posts");
    rig.runtime.next(); rig.worker.pump();
    rig.runtime.next(); // B уже оставил cached primer, но ещё не достиг ready
    check(rig.runtime.post({2, 0}), "C replaces incoming wish during priming");
    rig.runtime.next();
    for (uint32_t i = 0; i < 16; ++i) rig.worker.pump();
    const auto fresh = rig.runtime.next();
    check(fresh.left == int64_t{21} * fix32::ONE && fresh.right == int64_t{34} * fix32::ONE,
          "replacement drops the previous generation's cached primer");
    MusicMix last{};
    for (uint32_t i = 0; i < 16; ++i) { last = rig.runtime.next(); rig.worker.pump(); }
    check(last.underruns == 0 && last.left == int64_t{21} * fix32::ONE && last.right == int64_t{34} * fix32::ONE,
          "latest primed PCM preserves its loop after outgoing retirement");
    check(rig.runtime.post({2, 99}), "repeating current track is accepted");
    const MusicMix repeated = rig.runtime.next();
    check(repeated.underruns == 0 && repeated.left == int64_t{-55} * fix32::ONE && repeated.right == int64_t{89} * fix32::ONE,
          "repeated current track does not restart or drop the decoder");
}
void bounds() {
    using namespace audio;
    auto invalid = std::make_unique<MusicRuntime>(0);
    check(!invalid->post({0, 1}) && invalid->next().left == 0, "empty runtime disables wishes and output");
    auto over = std::make_unique<MusicRuntime>(4097);
    check(!over->post({0, 1}) && over->next().left == 0, "oversized catalogue disables runtime");
    auto maximum = std::make_unique<MusicRuntime>(4096);
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP; ++i) check(maximum->post({i, i}), "wish queue accepts capacity");
    check(!maximum->post({0, 0}) && !maximum->post({4096, 1}), "full queue and invalid track refuse");
    maximum->next();
    maximum->transport().closed.store(true, std::memory_order_release);
    check(!maximum->post({0, 0}) && maximum->next().underruns == 0, "closed runtime refuses wishes");
}
void repeated_ramps() {
    using namespace music_runtime_fixture;
    auto baseline = make_rig(), repeated = make_rig();
    check(baseline->ready && repeated->ready, "repeat fixtures ready");
    if (!baseline->ready || !repeated->ready) return;
    for (uint32_t t = 0; t < 350; ++t) {
        const bool event = t == 0 || t == 50 || t == 200;
        const audio::MusicWish wish = t < 50 || t >= 200 ? audio::MusicWish{0, 100} : audio::MusicWish{1, 1000};
        const auto plain = baseline->step(event, wish);
        const auto again = repeated->step(true, wish);
        check(same(plain, again), "current/incoming repeats preserve ramp and PCM position, including reverse");
    }
}
void hold_and_cancel() {
    using namespace audio;
    auto rig = music_runtime_fixture::make_rig();
    check(rig->ready, "hold fixture ready"); if (!rig->ready) return;
    rig->step(true, {0, 1000});
    for (uint32_t i = 0; i < 20; ++i) rig->step();
    check(rig->runtime.post({1, 500}), "delayed incoming accepted");
    const int64_t held_gain = fix32::ONE * int64_t{20} / 1000;
    for (uint32_t i = 20; i <= 50; ++i) {
        const auto mixed = rig->runtime.next();
        const auto pcm = music_runtime_fixture::sample(0, i);
        check(mixed.left == pcm.left * held_gain && mixed.right == pcm.right * held_gain && !mixed.underruns,
              "outgoing gain freezes while incoming decoder is not pumped");
    }
    check(rig->runtime.post({0, 1000}), "cancel priming by returning to outgoing");
    const auto returned = rig->runtime.next();
    const auto pcm = music_runtime_fixture::sample(0, 51);
    check(returned.left == pcm.left * held_gain && returned.right == pcm.right * held_gain,
          "cancelling priming preserves current gain and cursor");
    const auto next = rig->runtime.next();
    const auto next_pcm = music_runtime_fixture::sample(0, 52);
    const int64_t rising = held_gain + (fix32::ONE - held_gain) / 1000;
    check(next.left == next_pcm.left * rising && next.right == next_pcm.right * rising,
          "returned outgoing resumes toward unity");
}
void return_during_shortfall(bool return_old) {
    using namespace music_runtime_fixture;
    auto rig = make_rig();
    check(rig->ready, "shortfall return fixture ready"); if (!rig->ready) return;
    const auto gain = [](int64_t from, int64_t to, uint32_t elapsed, uint32_t length) {
        return from + (to - from) * std::min(elapsed, length) / length;
    };
    for (uint32_t f = 0; f < 2000; ++f) {
        if (f == 0) rig->runtime.post({0, 100});
        if (f == 200) rig->runtime.post({1, 1000});
        if (f == 400) rig->runtime.post({2, 500});
        if (f == 450) rig->runtime.post(return_old ? audio::MusicWish{0, 100} : audio::MusicWish{1, 1000});
        const auto mixed = rig->step();
        if (f < 400) continue;
        int64_t a = gain(52495, 0, f - 400, 480), b = 13041;
        if (return_old && f >= 450) {
            a = gain(47027, fix32::ONE, f - 450, 100);
            b = gain(13041, 0, f - 450, 100);
        } else if (!return_old && f >= 880) b = gain(13041, fix32::ONE, f - 880, 1000);
        const auto pa = sample(0, f - 1), pb = sample(1, f - 201);
        check(mixed.left == pa.left * a + pb.left * b && mixed.right == pa.right * a + pb.right * b && !mixed.underruns,
              "return during shortfall preserves positions and exact boundary gains");
    }
}
void churn_recovery() {
    using namespace audio;
    constexpr std::array<StereoFrame, 1> final_pcm{{{1234, -2345}}};
    auto catalogue = music_runtime_fixture::catalogue();
    std::array<MusicTrack, 5> tracks;
    std::copy(catalogue.begin(), catalogue.end(), tracks.begin());
    tracks[4].id = 5; tracks[4].pcm = final_pcm; tracks[4].frames = tracks[4].loop_end = 1;
    const std::array<uint32_t, 5> fades{0, 7, 1000, UINT32_MAX, 0};
    for (uint32_t i = 0; i < tracks.size(); ++i) tracks[i].fade_samples = fades[i];
    auto runtime = std::make_unique<MusicRuntime>(5);
    MusicWorker worker(runtime->transport()); std::string error;
    check(worker.init(tracks, error), "churn worker ready");
    uint32_t random = 13;
    for (uint32_t i = 0; i < 10000; ++i) {
        random = random * 1664525u + 1013904223u;
        if (i % 53 == 0) {
            const uint32_t track = (random >> 8) % 5;
            check(runtime->post({track, fades[track]}), "churn wish accepted");
        }
        if (i % 97 == 0) {
            auto& buffer = runtime->transport().slots[(random >> 12) % 2];
            const auto tag = buffer.ready.load();
            if (tag) { buffer.ready.store(0); buffer.failed.store(tag); }
        }
        const auto mixed = runtime->next();
        const int64_t bound = int64_t{2} * 32768 * fix32::ONE;
        check(mixed.left >= -bound && mixed.left <= bound && mixed.right >= -bound && mixed.right <= bound &&
              mixed.underruns <= MUSIC_SLOTS, "bounded PCM and underruns under churn");
        for (uint32_t p = 0; p < (random & 3u); ++p) worker.pump();
        check(runtime->transport().peak_decoders.load() <= MUSIC_SLOTS, "churn cannot open a third decoder");
    }
    check(runtime->post({4, 0}), "final stable cue accepted");
    for (uint32_t i = 0; i < 700; ++i) {
        const auto mixed = runtime->next();
        for (uint32_t p = 0; p < 16; ++p) worker.pump();
        if (i >= 600) check(mixed.left == int64_t{1234} * fix32::ONE && mixed.right == int64_t{-2345} * fix32::ONE && !mixed.underruns,
                            "stable cue recovers after arbitrary priming, failures and UINT32_MAX fades");
    }
}
}

int main() {
    transitions(); bounds(); repeated_ramps(); hold_and_cancel(); return_during_shortfall(false); return_during_shortfall(true); churn_recovery();
    std::puts(fails ? "audio-music-runtime: FAIL" :
                      "audio-music-runtime: PASS priming, latest wish, fade zero, repeat, bounds and closed");
    return fails ? 1 : 0;
}
