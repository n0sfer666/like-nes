#include "vorbis_reader.hpp"

#include <climits>
#include <mutex>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace audio {
namespace {
// Пин stb_vorbis переписывает global crc_table при КАЖДОМ open. Seek/length её читают.
// Только эти операции сериализуются; последовательное read не использует CRC. Всё вне RT.
std::mutex crc_mutex;
}
VorbisReader::~VorbisReader() { close(); }
void VorbisReader::close() {
    if (decoder_) stb_vorbis_close(decoder_);
    decoder_ = nullptr; info_ = {}; error_ = 0;
}
bool VorbisReader::open(std::span<const uint8_t> encoded, std::string& error) {
    close(); error.clear();
    if (encoded.empty() || !encoded.data() || encoded.size() > INT_MAX) {
        error_ = VORBIS_invalid_stream; error = "empty or oversized Vorbis buffer"; return false;
    }
    int code = 0;
    VorbisInfo pending;
    {
        std::lock_guard lock(crc_mutex);
        // C API читает заимствованный immutable encoded buffer; длина проверена до int cast.
        decoder_ = stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()), &code, nullptr);
        if (decoder_) {
            const auto info = stb_vorbis_get_info(decoder_);
            const auto frames = stb_vorbis_stream_length_in_samples(decoder_);
            code = stb_vorbis_get_error(decoder_);
            if (code == VORBIS__no_error && info.sample_rate && frames && info.channels > 0 && info.channels <= STB_VORBIS_MAX_CHANNELS &&
                stb_vorbis_seek_start(decoder_)) pending = {info.sample_rate, static_cast<uint32_t>(info.channels), frames};
            else if (code == VORBIS__no_error) code = VORBIS_invalid_stream;
        }
    }
    if (!decoder_ || !pending.frames) {
        close(); error_ = code; error = "cannot open nonempty Vorbis source (error " + std::to_string(code) + ")"; return false;
    }
    info_ = pending; error_ = 0; return true;
}
bool VorbisReader::seek(uint32_t frame) {
    if (!decoder_ || frame >= info_.frames) { error_ = VORBIS_seek_failed; return false; }
    std::lock_guard lock(crc_mutex);
    const bool ok = stb_vorbis_seek(decoder_, frame) != 0;
    error_ = stb_vorbis_get_error(decoder_);
    if (!ok && !error_) error_ = VORBIS_seek_failed;
    return ok && !error_;
}
bool VorbisReader::read(std::span<int16_t> output, uint32_t channels, uint32_t& frames) {
    frames = 0;
    if (!decoder_ || !output.data() || (channels != info_.channels && channels != 2) ||
        output.size() < channels || output.size() > INT_MAX) { error_ = VORBIS_invalid_api_mixing; return false; }
    const int got = stb_vorbis_get_samples_short_interleaved(decoder_, static_cast<int>(channels), output.data(), static_cast<int>(output.size()));
    error_ = stb_vorbis_get_error(decoder_);
    if (got < 0 || error_) return false;
    frames = static_cast<uint32_t>(got); return true;
}
}
