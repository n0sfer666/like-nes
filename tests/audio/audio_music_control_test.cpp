#include "music_control.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "music_worker.hpp"

namespace {
using namespace audio;

std::array<MusicTrack, 3> short_tracks() {
    static constexpr std::array<StereoFrame, 3> pcm{{{101, -101}, {202, -202}, {303, -303}}};
    std::array<MusicTrack, 3> tracks{};
    const std::array<uint64_t, 3> ids{UINT64_MAX, 0, 7};
    const std::array<uint32_t, 3> fades{7, 123, 0};
    for (uint32_t i = 0; i < tracks.size(); ++i) {
        tracks[i].id = ids[i]; tracks[i].pcm = std::span(&pcm[i], 1);
        tracks[i].frames = tracks[i].loop_end = 1; tracks[i].fade_samples = fades[i];
    }
    return tracks;
}

bool threshold_and_replay() {
    auto tracks = short_tracks();
    MusicControl control;
    std::string error;
    bool ok = control.init(tracks, 3, error);
    auto runtime = std::make_unique<MusicRuntime>(3);
    auto worker = std::make_unique<MusicWorker>(runtime->transport());
    ok &= worker->init(tracks, error);
    control.tick(8, *runtime); // До первого желания не выбирается каталог по умолчанию.
    ok &= control.want(0) && !control.want(99);
    control.tick(10, *runtime); control.tick(10, *runtime); control.tick(9, *runtime);
    runtime->next();
    MusicLoad load{};
    ok &= !runtime->transport().requests.peek(load);
    control.tick(11, *runtime); runtime->next();
    ok &= !runtime->transport().requests.peek(load);
    control.tick(12, *runtime); runtime->next();
    ok &= runtime->transport().requests.peek(load) && load.track == 1;
    for (uint32_t i = 0; i < 16; ++i) worker->pump();
    for (uint32_t i = 0; i <= 123; ++i) {
        const auto mixed = runtime->next();
        const int64_t gain = int64_t{fix32::ONE} * i / 123;
        ok &= mixed.left == 202 * gain && mixed.right == -202 * gain && !mixed.underruns;
    }
    return ok;
}

bool gap_wrap_and_reset() {
    auto tracks = short_tracks();
    MusicControl control;
    std::string error;
    bool ok = control.init(tracks, 3, error);
    auto runtime = std::make_unique<MusicRuntime>(3);
    auto worker = std::make_unique<MusicWorker>(runtime->transport());
    ok &= worker->init(tracks, error) && control.want(UINT64_MAX);
    control.tick(1, *runtime); control.tick(3, *runtime); control.tick(4, *runtime);
    runtime->next();
    MusicLoad load{};
    ok &= !runtime->transport().requests.peek(load);
    control.tick(5, *runtime); runtime->next();
    ok &= runtime->transport().requests.peek(load) && load.track == 0;
    worker->pump();
    for (uint32_t i = 0; i < 8; ++i) { runtime->next(); worker->pump(); }

    MusicControl wrap;
    ok &= wrap.init(tracks, 3, error) && wrap.want(7);
    auto wrapped = std::make_unique<MusicRuntime>(3);
    auto wrapped_worker = std::make_unique<MusicWorker>(wrapped->transport());
    ok &= wrapped_worker->init(tracks, error);
    wrap.tick(UINT32_MAX - 1, *wrapped); wrap.tick(UINT32_MAX, *wrapped); wrap.tick(0, *wrapped);
    wrapped->next();
    ok &= wrapped->transport().requests.peek(load) && load.track == 2;
    wrapped_worker->pump();
    for (uint32_t i = 0; i < 8; ++i) { wrapped->next(); wrapped_worker->pump(); }

    control.reset_intent();
    control.tick(100, *runtime); runtime->next();
    ok &= control.want(0);
    control.tick(0, *runtime); runtime->next();
    ok &= !runtime->transport().requests.peek(load); // Reset сохраняет порог и требует свежую эпоху.
    control.tick(1, *runtime); runtime->next();
    ok &= !runtime->transport().requests.peek(load);
    control.tick(2, *runtime); runtime->next();
    ok &= runtime->transport().requests.peek(load) && load.track == 1;
    return ok;
}

bool full_queue_keeps_latest() {
    constexpr uint32_t count = MUSIC_REQUEST_CAP + 2;
    static constexpr std::array<StereoFrame, count> pcm = [] {
        std::array<StereoFrame, count> result{};
        for (uint32_t i = 0; i < count; ++i) result[i] = {static_cast<int16_t>(100 + i), 0};
        return result;
    }();
    std::array<MusicTrack, count> tracks{};
    for (uint32_t i = 0; i < count; ++i) {
        tracks[i].id = i; tracks[i].pcm = std::span(&pcm[i], 1);
        tracks[i].frames = tracks[i].loop_end = 1; tracks[i].fade_samples = 0;
    }
    MusicControl control;
    std::string error;
    bool ok = control.init(tracks, 1, error);
    auto runtime = std::make_unique<MusicRuntime>(count);
    auto worker = std::make_unique<MusicWorker>(runtime->transport());
    ok &= worker->init(tracks, error);
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP; ++i) {
        ok &= control.want(i); control.tick(i, *runtime);
    }
    ok &= control.want(MUSIC_REQUEST_CAP); control.tick(20, *runtime);
    ok &= control.want(MUSIC_REQUEST_CAP + 1); control.tick(21, *runtime);
    runtime->next(); // Освобождаем место в command ring, оставляя последнее желание pending.
    control.tick(22, *runtime);
    runtime->next(); worker->pump();
    MusicMix mixed{};
    for (uint32_t i = 0; i < 16; ++i) { mixed = runtime->next(); worker->pump(); }
    ok &= mixed.left == int64_t{100 + MUSIC_REQUEST_CAP + 1} * 65536;
    return ok;
}

bool invalid_init_retry() {
    auto tracks = short_tracks();
    std::vector<MusicTrack> too_many(4097);
    MusicControl control;
    std::string error;
    bool ok = !control.init({}, 1, error) && !control.init(too_many, 1, error) &&
        !control.init(tracks, 0, error);
    auto duplicate = tracks;
    duplicate[1].id = duplicate[0].id;
    ok &= !control.init(duplicate, 1, error) && control.init(tracks, 1, error);
    return ok && !control.want(123);
}
bool reset_discards_only_unpublished() {
    constexpr uint32_t count = MUSIC_REQUEST_CAP + 1;
    static constexpr StereoFrame pcm{100, -100};
    std::array<MusicTrack, count> tracks{};
    for (uint32_t i = 0; i < count; ++i) {
        tracks[i].id = i; tracks[i].pcm = {&pcm, 1};
        tracks[i].frames = tracks[i].loop_end = 1; tracks[i].fade_samples = 0;
    }
    MusicControl control; std::string error;
    auto runtime = std::make_unique<MusicRuntime>(count);
    MusicWorker worker(runtime->transport());
    bool ok = control.init(tracks, 1, error) && worker.init(tracks, error);
    for (uint32_t i = 0; i < count; ++i) { ok &= control.want(i); control.tick(i, *runtime); }
    control.reset_intent();
    runtime->next();
    MusicLoad load;
    ok &= runtime->transport().requests.peek(load) && load.track == MUSIC_REQUEST_CAP - 1;
    worker.pump();
    control.tick(100, *runtime); runtime->next();
    ok &= !runtime->transport().requests.peek(load); // Старый pending не публикуется после reset без want.
    ok &= control.want(0); control.tick(0, *runtime); runtime->next();
    ok &= runtime->transport().requests.peek(load) && load.track == 0;
    return ok;
}
}

int main() {
    const bool ok = invalid_init_retry() && threshold_and_replay() && gap_wrap_and_reset() && full_queue_keeps_latest() && reset_discards_only_unpublished();
    std::puts(ok ? "audio-music-control: PASS selection, epochs, queue and IDs" :
        "audio-music-control: FAIL selection, epochs, queue and IDs");
    return ok ? 0 : 1;
}
