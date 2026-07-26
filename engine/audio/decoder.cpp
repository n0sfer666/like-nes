#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <cstdlib>

#include "decoder.hpp"

namespace audio {

bool decode_vorbis(const uint8_t* data, uint32_t size, DecodedPcm& out) {
    int channels = 0, rate = 0;
    short* pcm = nullptr;
    int frames = stb_vorbis_decode_memory(reinterpret_cast<const unsigned char*>(data),
                                          static_cast<int>(size), &channels, &rate, &pcm);
    if (frames < 0 || !pcm || channels < 1) {
        if (pcm) std::free(pcm);
        return false;
    }
    out.frames = static_cast<uint32_t>(frames);
    out.rate = static_cast<uint32_t>(rate);
    out.samples.resize(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        if (channels == 1) {
            out.samples[i] = pcm[i];
        } else {
            int32_t acc = 0; // даунмикс в mono (среднее каналов)
            for (int c = 0; c < channels; ++c) acc += pcm[i * channels + c];
            out.samples[i] = static_cast<int16_t>(acc / channels);
        }
    }
    std::free(pcm);
    return true;
}

} // namespace audio
