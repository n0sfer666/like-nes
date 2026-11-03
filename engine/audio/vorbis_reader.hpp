#pragma once
#include <cstdint>
#include <span>
#include <string>

struct stb_vorbis;

namespace audio {
struct VorbisInfo { uint32_t rate = 0, channels = 0, frames = 0; };
// Вне RT. Один поток владеет экземпляром; разные экземпляры можно использовать параллельно.
// Encoded buffer заимствован и неизменен до close/destroy. Непустой output — interleaved int16.
class VorbisReader {
public:
    VorbisReader() = default;
    ~VorbisReader();
    VorbisReader(const VorbisReader&) = delete;
    VorbisReader& operator=(const VorbisReader&) = delete;
    bool open(std::span<const uint8_t> encoded, std::string& error);
    void close();
    bool seek(uint32_t frame); // frame < info().frames; неверный аргумент не двигает cursor
    // channels = native info().channels либо 2 (stereo); mono downmix делает decoder.hpp.
    bool read(std::span<int16_t> output, uint32_t channels, uint32_t& frames); // EOF: true,frames=0
    VorbisInfo info() const { return info_; }
    int last_error() const { return error_; }
private:
    stb_vorbis* decoder_ = nullptr;
    VorbisInfo info_;
    int error_ = 0;
};
// Полный проход без хранения PCM: подтверждает declared frames реальным декодом.
// Отказ очищает out; codec/rate restrictions музыки проверяет вызывающий.
bool inspect_vorbis(std::span<const uint8_t> encoded, VorbisInfo& out, std::string& error);
}
