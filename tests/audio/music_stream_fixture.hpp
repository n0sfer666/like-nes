#pragma once
#include <array>
#include <memory>
#include <string>

#include "mixer.hpp"
#include "music_runtime.hpp"
#include "music_stream.hpp"
#include "music_worker.hpp"

namespace music_stream_fixture {
inline constexpr std::array<audio::StereoFrame, 5> A{{{100, -200}, {300, -400}, {500, -600}, {700, -800}, {900, -1000}}};
inline constexpr std::array<audio::StereoFrame, 4> B{{{-110, 220}, {-330, 440}, {-550, 660}, {-770, 880}}};
inline constexpr std::array<audio::StereoFrame, 4> C{{{21, 34}, {-55, 89}, {144, -233}, {377, -610}}};
inline constexpr std::array<audio::StereoFrame, 6> D{{{80, 90}, {-100, -110}, {120, 130}, {-140, -150}, {160, 170}, {-180, -190}}};

inline std::array<audio::MusicTrack, 4> catalogue() {
    std::array<audio::MusicTrack, 4> tracks{};
    const std::array<std::span<const audio::StereoFrame>, 4> pcm{A, B, C, D};
    const std::array<uint32_t, 4> starts{1, 2, 0, 1}, ends{4, 4, 4, 5};
    const std::array<uint32_t, 4> fades{100, 1000, 500, 600};
    for (uint32_t i = 0; i < tracks.size(); ++i) {
        tracks[i].id = i == 0 ? 0 : (i == 1 ? UINT64_MAX : i + 10);
        tracks[i].pcm = pcm[i];
        tracks[i].frames = static_cast<uint32_t>(pcm[i].size());
        tracks[i].loop_start = starts[i];
        tracks[i].loop_end = ends[i];
        tracks[i].fade_samples = fades[i];
    }
    return tracks;
}

struct ManualRig {
    std::array<audio::MusicTrack, 4> tracks = catalogue();
    std::shared_ptr<audio::MusicRuntime> runtime = std::make_shared<audio::MusicRuntime>(4);
    std::unique_ptr<audio::MusicWorker> worker;
    std::unique_ptr<audio::Mixer> mixer;
    bool ready = false, bound = false;
    explicit ManualRig(audio::Backend backend) {
        worker = std::make_unique<audio::MusicWorker>(runtime->transport());
        mixer = std::make_unique<audio::Mixer>(backend);
        std::string error;
        ready = worker->init(tracks, error);
        bound = ready && audio::MusicRuntime::bind(*mixer, runtime);
    }
    void pump(uint32_t count = 16) { for (uint32_t i = 0; i < count; ++i) worker->pump(); }
};

inline audio::StereoFrame sample(uint32_t track, uint64_t cursor) {
    const auto tracks = catalogue();
    const auto& source = tracks[track];
    const uint32_t frame = cursor < source.loop_end
        ? static_cast<uint32_t>(cursor)
        : source.loop_start + static_cast<uint32_t>((cursor - source.loop_end) % (source.loop_end - source.loop_start));
    return source.pcm[frame];
}
} // namespace music_stream_fixture
