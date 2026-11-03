#include "decoder.hpp"

#include <climits>
#include <utility>

#include "vorbis_reader.hpp"

namespace audio {
namespace {

bool decode(std::span<const uint8_t> data, DecodedPcm& out) {
    VorbisReader reader; std::string error;
    if (!reader.open(data, error)) return false;
    const auto info = reader.info();
    std::vector<int16_t> block(static_cast<size_t>(info.channels) * 2048);
    for (;;) {
        uint32_t frames = 0;
        if (!reader.read(block, info.channels, frames) || uint64_t{out.frames} + frames > info.frames) return false;
        if (!frames) break;
        const size_t begin = out.frames;
        out.frames += frames; out.samples.resize(out.frames);
        for (uint32_t i = 0; i < frames; ++i) {
            int32_t sum = 0;
            for (uint32_t c = 0; c < info.channels; ++c) sum += block[static_cast<size_t>(i) * info.channels + c];
            out.samples[begin + i] = static_cast<int16_t>(sum / static_cast<int32_t>(info.channels));
        }
    }
    if (out.frames != info.frames) return false;
    out.rate = info.rate; return true;
}
}

bool decode_vorbis(const uint8_t* data, uint32_t size, DecodedPcm& out) {
    DecodedPcm result;
    // Выход меняется после закрытия reader: encoded data может лежать в старом out.samples.
    const bool ok = data && size && size <= INT_MAX && decode({data, size}, result);
    out = ok ? std::move(result) : DecodedPcm{};
    return ok;
}

} // namespace audio
