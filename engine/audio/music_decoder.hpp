#pragma once
#include "music_track.hpp"
#include "vorbis_reader.hpp"

namespace audio {
// Только worker/cold setup, один owning thread. Одинаковые loop/partial-block правила для
// Vorbis и синтетического PCM. Не экспортируется SDK: публичный handle будет у MusicStream.
class MusicDecoder {
public:
    static constexpr uint32_t BLOCK_FRAMES = 1024;
    bool open(const MusicTrack& track, std::string& error);
    void close();
    // false/frames0: весь out отбрасывается, включая уже записанный префикс ошибочного блока.
    bool read_loop(std::span<StereoFrame> out, uint32_t& frames, std::string& error);
    bool valid() const { return track_.frames != 0; }
    uint32_t cursor() const { return cursor_; }
private:
    MusicTrack track_;
    VorbisReader vorbis_;
    uint32_t cursor_ = 0;
};
}
