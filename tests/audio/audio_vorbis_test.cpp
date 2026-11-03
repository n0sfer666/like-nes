#include "vorbis_fixture.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "decoder.hpp"
#include "vorbis_reader.hpp"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace {
using vorbis_fixture::check;
void source(const char* path, uint32_t rate, uint32_t channels, uint32_t frames) {
    const auto bytes = vorbis_fixture::load(path);
    if (bytes.empty()) return;
    // Старый one-shot C API — независимый от chunk/read/seek wrapper oracle на этой ОС.
    short* pcm = nullptr; int native_channels = 0, native_rate = 0;
    const int count = stb_vorbis_decode_memory(bytes.data(), static_cast<int>(bytes.size()), &native_channels, &native_rate, &pcm);
    check(count == static_cast<int>(frames) && native_channels == static_cast<int>(channels) && native_rate == static_cast<int>(rate) && pcm, "native reference shape");
    if (!pcm || count != static_cast<int>(frames) || native_channels != static_cast<int>(channels)) { std::free(pcm); return; }
    audio::VorbisInfo info; std::string error;
    check(audio::inspect_vorbis(bytes, info, error) && error.empty() && info.frames == frames && info.channels == channels && info.rate == rate, "full source inspection");
    {
        audio::VorbisReader reader; std::array<int16_t, 128> stereo{}; uint32_t got = 0;
        check(reader.open(bytes, error) && reader.read(stereo, 2, got) && got == 64, "stereo output for mono and stereo sources");
        for (uint32_t i = 0; i < got; ++i) {
            check(stereo[i * 2] == pcm[i * channels] && stereo[i * 2 + 1] == pcm[i * channels + channels - 1], "stereo conversion preserves independent channels or duplicates mono");
        }
    }
    for (uint32_t block : {1u, 31u, 257u, 1024u}) {
        audio::VorbisReader reader;
        check(reader.open(bytes, error), "open reader");
        std::vector<int16_t> output(static_cast<size_t>(block) * channels);
        uint32_t cursor = 0, got = 0;
        while (reader.read(output, channels, got) && got) {
            check(cursor + got <= frames && std::equal(output.begin(), output.begin() + got * channels, pcm + cursor * channels), "every native sample matches one-shot");
            cursor += got;
            if (cursor > frames) break;
        }
        check(cursor == frames && !reader.last_error(), "exact EOF after arbitrary blocks");
        check(reader.read(output, channels, got) && got == 0, "repeated EOF is successful silence");
        for (uint32_t frame : {0u, 37u, 1023u, frames - 1}) {
            check(reader.seek(frame), "sample-accurate seek");
            check(reader.read(output, channels, got) && got == std::min(block, frames - frame) &&
                std::equal(output.begin(), output.begin() + got * channels, pcm + frame * channels), "seek matches reference including last sample");
        }
        check(reader.seek(37) && !reader.seek(frames) && !reader.seek(UINT32_MAX), "invalid seek refused");
        check(reader.read(output, channels, got) && got == std::min(block, frames - 37) &&
            std::equal(output.begin(), output.begin() + got * channels, pcm + 37 * channels), "invalid seek preserves cursor");
    }
    audio::DecodedPcm mono;
    check(audio::decode_vorbis(bytes.data(), static_cast<uint32_t>(bytes.size()), mono) && mono.rate == rate && mono.frames == frames, "legacy decode shape");
    if (mono.samples.size() == frames) for (uint32_t i = 0; i < frames; ++i) {
        int32_t sum = 0;
        for (uint32_t c = 0; c < channels; ++c) sum += pcm[i * channels + c];
        check(mono.samples[i] == sum / static_cast<int32_t>(channels), "legacy average remains exact");
    }
    else check(false, "legacy sample count");
    // Заимствованный encoded input может занимать старую vector выхода до commit результата.
    audio::DecodedPcm alias;
    alias.samples.resize((bytes.size() + 1) / 2);
    std::memcpy(alias.samples.data(), bytes.data(), bytes.size());
    // C API принимает байты: представление собственной int16 vector, живущей до возврата.
    const auto* encoded = reinterpret_cast<const uint8_t*>(alias.samples.data());
    check(audio::decode_vorbis(encoded, static_cast<uint32_t>(bytes.size()), alias) && alias.samples == mono.samples, "input alias survives output replacement");
    std::free(pcm);
}
void refusal() {
    const auto bytes = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    audio::VorbisReader reader; std::string error;
    std::array<int16_t, 128> output{}; uint32_t frames = 99;
    check(!reader.read(output, 2, frames) && !frames, "unopened read resets count");
    check(reader.open(bytes, error), "refusal fixture");
    std::array<int16_t, 1> mono{};
    check(!reader.read(mono, 1, frames), "stereo-to-mono C conversion refused before vendor overflow");
    check(!reader.read({}, 2, frames) && !reader.read(output, 0, frames) && !reader.read(output, 3, frames) && !reader.read(output, 17, frames) &&
        !reader.read(std::span(output).first(1), 2, frames), "invalid output layout refused");
    check(reader.read(output, 2, frames) && frames == 64, "invalid reads preserve cursor");
    check(!reader.open({}, error) && !error.empty() && !reader.info().frames && !reader.seek(0), "failed reopen closes old state");
    const uint8_t one = 0;
    check(!reader.open({&one, static_cast<size_t>(INT_MAX) + 1}, error), "oversize refused before C API cast");
    for (size_t cut : {size_t{0}, size_t{4}, bytes.size() / 2}) {
        audio::VorbisInfo info{1, 2, 3};
        check(!audio::inspect_vorbis(std::span(bytes).first(cut), info, error) && !error.empty() && !info.rate && !info.channels && !info.frames, "truncated inspect clears result");
    }
    audio::DecodedPcm old{{1, 2}, 2, 48000};
    check(!audio::decode_vorbis(&one, UINT32_MAX, old) && old.samples.empty() && !old.frames && !old.rate, "oversize legacy clears result");
    check(!audio::decode_vorbis(nullptr, 1, old) && !audio::decode_vorbis(&one, 1, old), "invalid legacy refuses");
    check(reader.open(bytes, error) && error.empty(), "recovery after failures");
    reader.close(); reader.close(); check(!reader.info().frames && !reader.last_error(), "idempotent close");
}
}
int main() {
    source("tests/audio/fixtures/stereo-48000.ogg", 48000, 2, 4800);
    source("tests/audio/fixtures/stereo-44100.ogg", 44100, 2, 4410);
    source("engine/audio/assets/src/music.ogg", 48000, 1, 96000);
    source("engine/audio/assets/src/sfx_ping.ogg", 48000, 1, 14400);
    refusal();
    std::puts(vorbis_fixture::fails ? "audio-vorbis: FAIL" : "audio-vorbis: PASS exact samples, seek, inspection, aliasing and refusal");
    return vorbis_fixture::fails ? 1 : 0;
}
