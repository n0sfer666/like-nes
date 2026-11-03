#include "music_decoder.hpp"

#include <algorithm>
#include <array>

namespace audio {
void MusicDecoder::close() { vorbis_.close(); track_ = {}; cursor_ = 0; }
bool MusicDecoder::open(const MusicTrack& track, std::string& error) {
    close(); error.clear();
    auto fail = [&](const char* message) { close(); error = message; return false; };
    if (track.encoded.empty() == track.pcm.empty()) return fail("music needs exactly one encoded or stereo PCM source");
    if (!track.frames || track.loop_start >= track.loop_end || track.loop_end > track.frames)
        return fail("music metadata has invalid source length or loop bounds");
    if (!track.pcm.empty()) {
        if (!track.pcm.data() || track.pcm.size() != track.frames) return fail("music PCM length differs from metadata");
    } else {
        if (!vorbis_.open(track.encoded, error)) return false;
        const auto info = vorbis_.info();
        if (info.rate != SAMPLE_RATE || (info.channels != 1 && info.channels != 2) || info.frames != track.frames)
            return fail("music Vorbis needs48000Hz mono/stereo and the exact metadata length");
    }
    track_ = track; return true;
}
bool MusicDecoder::read_loop(std::span<StereoFrame> out, uint32_t& frames, std::string& error) {
    frames = 0; error.clear();
    if (!valid() || out.empty() || !out.data()) { error = "music decoder needs an open source and output frames"; return false; }
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(out.size(), BLOCK_FRAMES));
    // Никакого int16 pointer cast через поля StereoFrame: собственный interleaved C-API buffer.
    std::array<int16_t, BLOCK_FRAMES * 2> interleaved;
    while (frames < count) {
        if (cursor_ == track_.loop_end) {
            if (!track_.encoded.empty() && !vorbis_.seek(track_.loop_start)) {
                error = "music loop seek failed"; frames = 0; close(); return false;
            }
            cursor_ = track_.loop_start;
        }
        const uint32_t n = std::min(count - frames, track_.loop_end - cursor_);
        if (!track_.pcm.empty()) std::copy_n(track_.pcm.data() + cursor_, n, out.data() + frames);
        else {
            uint32_t got = 0;
            if (!vorbis_.read(std::span(interleaved).first(n * 2), 2, got) || got != n) {
                error = "music source ended before its loop boundary"; frames = 0; close(); return false;
            }
            for (uint32_t i = 0; i < n; ++i) out[frames + i] = {interleaved[i * 2], interleaved[i * 2 + 1]};
        }
        frames += n; cursor_ += n;
    }
    return true;
}
}
