#include "music_runtime_fixture.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#include "music_ogg_corruption.hpp"
#include "vorbis_fixture.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

namespace {
int fails;
void check(bool ok, const char* name) {
    if (!ok) { ++fails; std::printf("audio-music-runtime-limits: FAIL %s\n", name); }
}
void stream_underrun() {
    auto owner = music_runtime_fixture::make_rig(); auto& rig = *owner;
    check(rig.ready, "worker init");
    if (!rig.ready) return;
    rig.runtime.post({0, 0});
    rig.runtime.next();
    for (uint32_t i = 0; i < 16; ++i) rig.worker.pump();
    check(rig.runtime.next().underruns == 0, "ready primer is not an underrun");
    uint32_t missing = 0;
    for (uint32_t i = 0; i < 17000; ++i) missing += rig.runtime.next().underruns;
    check(missing == 17000 - (audio::StereoRing::CAPACITY - 1), "empty active PCM ring counts exact missing slot frames");
}
void full_worker_queue() {
    using namespace audio;
    auto owner = music_runtime_fixture::make_rig(); auto& rig = *owner;
    check(rig.ready, "worker init for full queue retry");
    if (!rig.ready) return;
    rig.step(true, {0, 0}); rig.step();
    const uint64_t original = rig.runtime.transport().slots[0].ready.load();
    rig.step(true, {1, 0}); rig.runtime.next(); rig.runtime.next();
    MusicLoad stop;
    check(rig.runtime.transport().requests.peek(stop) && stop.slot == 0 &&
          stop.track == MUSIC_STOP_TRACK && stop.generation > original, "retired slot gets a fresh stop tag");
    // Worker стоит; смены ещё не готового incoming заполняют очередь настоящими load requests.
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP + 4; ++i) {
        check(rig.runtime.post({i % 2 ? 2u : 3u, 0}), "replacement wish accepted");
        rig.runtime.next();
    }
    rig.worker.pump(); rig.runtime.next(); // место освобождено: повторяется самый последний load
    for (uint32_t i = 0; i < 16; ++i) rig.worker.pump();
    const auto first = rig.runtime.next();
    check(first.left == int64_t{21} * fix32::ONE && first.right == int64_t{34} * fix32::ONE && !first.underruns,
          "full request queue preserves latest load, discards stale PCM and starts the exact intro");
}
void failed_generation() {
    using namespace audio;
    auto encoded = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    check(music_ogg_corruption::apply(encoded), "corrupt later Ogg packet");
    auto tracks = music_runtime_fixture::catalogue();
    tracks[1].pcm = {};
    tracks[1].encoded = encoded;
    tracks[1].frames = tracks[1].loop_end = 4800;
    tracks[1].loop_start = 0;
    auto owner = std::make_unique<MusicRuntime>(2); auto& runtime = *owner;
    MusicWorker worker(runtime.transport());
    std::string error;
    check(worker.init(std::span(tracks).first(2), error), "worker accepts Ogg which fails after valid intro blocks");
    runtime.post({0, 0}); runtime.next();
    for (uint32_t i = 0; i < 16; ++i) worker.pump();
    runtime.next();
    runtime.post({1, 500}); runtime.next();
    for (uint32_t i = 0; i < 6; ++i) {
        worker.pump(); const auto mixed = runtime.next();
        const auto pcm = music_runtime_fixture::sample(0, i + 2);
        check(mixed.left == int64_t{pcm.left} * fix32::ONE && mixed.right == int64_t{pcm.right} * fix32::ONE && !mixed.underruns,
              "failed priming tag never mixes its cached or buffered PCM");
    }
    check(runtime.transport().failures.load() == 1 && runtime.transport().slots[1].ready.load() > 2,
          "failed generation is retired with a fresh stop tag");
    runtime.transport().closed.store(true, std::memory_order_release);
    worker.pump();
}
void failed_held_slot() {
    using namespace audio;
    auto rig = music_runtime_fixture::make_rig();
    check(rig->ready, "held failure fixture ready"); if (!rig->ready) return;
    rig->step(true, {0, 0}); rig->step();
    rig->step(true, {1, 1000});
    for (uint32_t i = 0; i < 128; ++i) rig->step();
    rig->step(true, {2, 500});
    for (uint32_t i = 0; i < 32; ++i) rig->step();
    auto& held = rig->runtime.transport().slots[1];
    const uint64_t failed = held.ready.load(std::memory_order_acquire);
    check(failed != 0, "held B was ready before failure");
    held.ready.store(0, std::memory_order_release);
    held.failed.store(failed, std::memory_order_release);
    rig->runtime.next();
    for (uint32_t i = 0; i < 16; ++i) rig->worker.pump();
    for (uint32_t i = 0; i < 700; ++i) {
        const auto mixed = rig->runtime.next(); rig->worker.pump();
        if (i >= 500) {
            const auto pcm = music_runtime_fixture::sample(2, i);
            check(mixed.left == int64_t{pcm.left} * fix32::ONE && mixed.right == int64_t{pcm.right} * fix32::ONE && !mixed.underruns,
                  "held decoder failure cannot strand the pending third cue");
        }
    }
    check(rig->runtime.transport().peak_decoders.load() <= MUSIC_SLOTS, "failure recovery preserves decoder budget");
}
void held_zero_gain() {
    using namespace audio;
    constexpr std::array<StereoFrame, 7> second{{{10,11},{20,21},{30,31},{40,41},{50,51},{60,61},{70,71}}};
    auto tracks = music_runtime_fixture::catalogue();
    tracks[1].pcm = second; tracks[1].frames = tracks[1].loop_end = 7;
    tracks[1].loop_start = 1; tracks[1].fade_samples = UINT32_MAX;
    auto runtime = std::make_unique<MusicRuntime>(3);
    MusicWorker worker(runtime->transport()); std::string error;
    check(worker.init(std::span(tracks).first(3), error), "zero gain held fixture ready");
    uint64_t tag = 0;
    bool alive = true, generation_kept = true, position_kept = true;
    for (uint32_t f = 0; f < 66032; ++f) {
        if (f == 0) runtime->post({0, 0});
        if (f == 2 || f == 14) runtime->post({1, UINT32_MAX});
        if (f == 4) runtime->post({2, 0});
        const auto mixed = runtime->next();
        for (uint32_t p = 0; p < (f <= 4 ? 16u : 1u); ++p) worker.pump();
        if (f == 3) tag = runtime->transport().slots[1].ready.load();
        if (f >= 4 && f < 484) alive &= runtime->transport().live_decoders.load() == 2;
        if (f >= 4) generation_kept &= runtime->transport().slots[1].ready.load() == tag;
        if (f >= 66020) {
            const uint32_t cursor = f - 3, index = cursor < 7 ? cursor : 1 + (cursor - 7) % 6;
            const int64_t gain = int64_t{fix32::ONE} * (f - 484) / UINT32_MAX;
            position_kept &= mixed.left == second[index].left * gain && mixed.right == second[index].right * gain && !mixed.underruns;
        }
    }
    check(tag != 0 && alive, "zero Q16 held gain must keep both decoders during shortfall");
    check(generation_kept, "return from zero held gain never reloads its decoder");
    check(position_kept, "PCM cursor is preserved through zero gain and becomes observable at Q16 gain1");
}
void closed_primer() {
    using namespace audio;
    auto rig = music_runtime_fixture::make_rig();
    check(rig->ready, "close fixture ready"); if (!rig->ready) return;
    rig->runtime.post({0, 0}); rig->runtime.next(); rig->worker.pump();
    rig->runtime.next(); // первый matching frame уже в primer, ready ещё нет
    for (uint32_t i = 0; i < 16; ++i) rig->worker.pump();
    check(rig->runtime.transport().slots[0].ready.load() != 0, "ready published after cached primer");
    rig->runtime.transport().closed.store(true, std::memory_order_release);
    const auto stopped = rig->runtime.next();
    check(stopped.left == 0 && stopped.right == 0 && !stopped.underruns && !rig->runtime.post({1, 0}),
          "closed overrides a cached ready primer and full ring");
    check(!rig->worker.pump() && rig->runtime.transport().live_decoders.load() == 0,
          "worker closes without waiting for any callback acknowledgment");
}
void allocation_budget() {
    auto rig = music_runtime_fixture::make_rig();
    check(rig->ready, "allocation fixture ready"); if (!rig->ready) return;
    framework::probe::in_hot = true;
    const bool controls = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(controls && framework::probe::allocs >= 2, "plain and aligned allocator controls");
    framework::probe::allocs = 0;
    uint32_t underruns = 0;
    for (uint32_t i = 0; i < 2300; ++i) {
        if (i == 0) rig->runtime.post({0, 0});
        if (i == 64) rig->runtime.post({1, 1000});
        if (i == 128 || i == 1000) rig->runtime.post({2, 500});
        if (i == 256 || i == 1500) rig->runtime.post({3, 600});
        if (i == 384 || i == 1100) rig->runtime.post({1, 1000});
        // Worker здесь ручной и последовательно вызывается вне горячего региона.
        framework::probe::in_hot = true;
        const auto mixed = rig->runtime.next();
        framework::probe::in_hot = false;
        underruns += mixed.underruns;
        for (uint32_t pump = 0; pump < 16; ++pump) rig->worker.pump();
    }
    check(framework::probe::allocs == 0 && underruns == 0, "all controller transitions allocate nothing in RT");
}
}

int main() {
    stream_underrun(); full_worker_queue(); failed_generation(); failed_held_slot(); held_zero_gain(); closed_primer(); allocation_budget();
    std::puts(fails || vorbis_fixture::fails ? "audio-music-runtime-limits: FAIL" :
        "audio-music-runtime-limits: PASS underruns, failure generations, Ogg refusal and shutdown");
    return fails || vorbis_fixture::fails ? 1 : 0;
}
