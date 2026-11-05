#pragma once
#include <array>
#include <memory>
#include <string>

#include "music_runtime.hpp"
#include "music_worker.hpp"

namespace music_runtime_fixture {
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
        tracks[i].id = i + 1;
        tracks[i].pcm = pcm[i];
        tracks[i].frames = static_cast<uint32_t>(pcm[i].size());
        tracks[i].loop_start = starts[i];
        tracks[i].loop_end = ends[i];
        tracks[i].fade_samples = fades[i];
    }
    return tracks;
}

inline audio::StereoFrame sample(uint32_t track, uint64_t cursor) {
    const auto tracks = catalogue();
    const auto& source = tracks[track];
    const uint32_t frame = cursor < source.loop_end
        ? static_cast<uint32_t>(cursor)
        : source.loop_start + static_cast<uint32_t>((cursor - source.loop_end) % (source.loop_end - source.loop_start));
    return source.pcm[frame];
}

struct Rig {
    audio::MusicRuntime runtime{4};
    audio::MusicWorker worker{runtime.transport()};
    std::array<audio::MusicTrack, 4> tracks = catalogue();
    bool ready = false;
    Rig() { std::string error; ready = worker.init(tracks, error); }
    audio::MusicMix step(bool post = false, audio::MusicWish wish = {}) {
        if (post) runtime.post(wish);
        const auto mixed = runtime.next();
        for (uint32_t i = 0; i < 16; ++i) worker.pump();
        return mixed;
    }
};
inline std::unique_ptr<Rig> make_rig() {
    // Два кольца занимают около0.5MiB: несколько fixtures не помещаются в Windows stack1MiB.
    return std::make_unique<Rig>();
}
inline bool same(audio::MusicMix a, audio::MusicMix b) {
    return a.left == b.left && a.right == b.right && a.underruns == b.underruns;
}
} // namespace music_runtime_fixture
