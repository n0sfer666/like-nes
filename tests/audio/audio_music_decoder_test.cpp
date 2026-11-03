#include "music_decoder.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include "vorbis_fixture.hpp"

namespace {
using vorbis_fixture::check;
using audio::StereoFrame;
bool exact(StereoFrame a, StereoFrame b) { return a.left == b.left && a.right == b.right; }
void compare(const audio::MusicTrack& track, std::span<const StereoFrame> reference, uint32_t total) {
    for (uint32_t block : {1u, 17u, 1024u, 4096u}) {
        audio::MusicDecoder decoder; std::string error;
        check(decoder.open(track, error) && error.empty(), "open loop source");
        std::vector<StereoFrame> output(block);
        uint32_t cursor = 0, produced = 0;
        while (produced < total) {
            uint32_t got = 0;
            check(decoder.read_loop(output, got, error) && got == std::min(block, audio::MusicDecoder::BLOCK_FRAMES) && error.empty(), "bounded partial loop block");
            if (!got) break;
            for (uint32_t i = 0; i < got; ++i) {
                if (cursor == track.loop_end) cursor = track.loop_start;
                check(exact(output[i], reference[cursor]), "every intro and loop sample matches independent source indexing");
                ++cursor;
            }
            produced += got;
        }
        check(produced >= total && decoder.valid(), "loop continues beyond source and two ring capacities");
    }
}
void ogg(const char* name, uint32_t start, uint32_t end) {
    const auto bytes = vorbis_fixture::load(name);
    audio::VorbisReader reference; std::string error;
    if (!reference.open(bytes, error)) { check(false, "reference Vorbis open"); return; }
    const auto info = reference.info();
    std::vector<int16_t> native(static_cast<size_t>(info.frames) * 2); uint32_t got = 0;
    check(reference.read(native, 2, got) && got == info.frames, "full source reference without loop seek");
    std::vector<StereoFrame> frames(info.frames);
    for (uint32_t i = 0; i < info.frames; ++i) frames[i] = {native[i * 2], native[i * 2 + 1]};
    audio::MusicTrack track; track.encoded = bytes; track.frames = info.frames; track.loop_start = start; track.loop_end = end;
    compare(track, frames, 33000);
    // Начало/конец не кратны decoder batch; также одноточечный луп без потери первого кадра.
    track.loop_start = 37; track.loop_end = 38;
    compare(track, frames, 117);
}
void pcm() {
    const std::array<StereoFrame, 5> frames{{{100, -200}, {300, -400}, {500, -600}, {700, -800}, {900, -1000}}};
    audio::MusicTrack track; track.id = UINT64_MAX; track.pcm = frames; track.frames = 5; track.loop_start = 1; track.loop_end = 4;
    compare(track, frames, 33000);
    track.loop_start = 4; track.loop_end = 5; compare(track, frames, 33000);
    track.loop_start = 0; track.loop_end = 5; compare(track, frames, 33000);
    audio::MusicDecoder decoder; std::string error; std::array<StereoFrame, 2> out; uint32_t got = 0;
    check(decoder.open(track, error) && decoder.read_loop(out, got, error) && got == 2 && decoder.cursor() == 2, "PCM cursor");
    check(!decoder.read_loop({}, got, error) && !got && !error.empty() && decoder.cursor() == 2 && decoder.valid(), "empty output refuses without changing cursor");
    check(decoder.read_loop(out, got, error) && got == 2 && exact(out[0], frames[2]) && error.empty(), "read recovers after invalid output");
    for (uint32_t bad = 0; bad < 6; ++bad) {
        auto invalid = track;
        if (bad == 0) invalid.frames = 0;
        if (bad == 1) invalid.loop_start = invalid.loop_end;
        if (bad == 2) invalid.loop_end = 6;
        if (bad == 3) invalid.frames = 4;
        if (bad == 4) invalid.pcm = {};
        const uint8_t byte = 0;
        if (bad == 5) invalid.encoded = {&byte, 1};
        check(!decoder.open(invalid, error) && !error.empty() && !decoder.valid() && decoder.cursor() == 0, "invalid source metadata closes previous state");
    }
    check(!decoder.read_loop(out, got, error) && !got, "closed decoder returns zero frames");
    check(decoder.open(track, error) && decoder.read_loop(out, got, error) && exact(out[0], frames[0]), "reopen starts intro from0");
    decoder.close(); decoder.close(); check(!decoder.valid(), "idempotent close");
}
void refusals() {
    for (const auto* file : {"tests/audio/fixtures/stereo-44100.ogg", "tests/audio/fixtures/three-48000.ogg"}) {
        const auto bytes = vorbis_fixture::load(file);
        audio::VorbisReader probe; std::string error;
        check(probe.open(bytes, error), "unsupported source itself is a real Ogg");
        audio::MusicTrack track; track.encoded = bytes; track.frames = probe.info().frames; track.loop_end = track.frames;
        audio::MusicDecoder decoder;
        check(!decoder.open(track, error) && !decoder.valid() && error.find("48000Hz mono/stereo") != std::string::npos, "wrong rate or channel count refused");
    }
    auto bytes = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    audio::MusicTrack track; track.encoded = bytes; track.frames = 4801; track.loop_end = 4000;
    audio::MusicDecoder decoder; std::string error;
    check(!decoder.open(track, error) && !decoder.valid(), "source length must match metadata even if loop fits");
    track.frames = 4800; bytes.resize(bytes.size() / 2); track.encoded = bytes;
    check(!decoder.open(track, error) && !decoder.valid(), "truncated source refused");
}
void read_failure() {
    auto bytes = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    size_t page = 0;
    for (uint32_t index = 0; index < 3; ++index) {
        if (page + 27 > bytes.size()) { check(false, "corruption fixture page header"); return; }
        const size_t body = page + 27 + bytes[page + 26];
        if (body > bytes.size()) { check(false, "corruption fixture lacing"); return; }
        size_t end = body;
        for (size_t i = page + 27; i < body; ++i) end += bytes[i];
        if (end > bytes.size()) { check(false, "corruption fixture page body"); return; }
        if (index != 2) { page = end; continue; }
        size_t packet_start = body, cursor = body; uint32_t packet = 0; bool changed = false;
        for (size_t i = page + 27; i < body; ++i) {
            cursor += bytes[i];
            if (bytes[i] == 255) continue;
            if (packet == 2 && packet_start < cursor) { bytes[packet_start] |= 1; changed = true; break; }
            ++packet; packet_start = cursor;
        }
        check(changed, "third audio packet changed after valid initial packets");
        // Пересчитываем Ogg CRC: контейнер и declared length остаются читаемыми, payload битый.
        std::fill_n(bytes.data() + page + 22, 4, 0);
        uint32_t crc = 0;
        for (size_t i = page; i < end; ++i) {
            crc ^= uint32_t{bytes[i]} << 24;
            for (uint32_t bit = 0; bit < 8; ++bit) crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04c11db7u : 0u);
        }
        for (uint32_t i = 0; i < 4; ++i) bytes[page + 22 + i] = static_cast<uint8_t>(crc >> (i * 8));
    }
    audio::MusicTrack track; track.encoded = bytes; track.frames = track.loop_end = 4800;
    audio::MusicDecoder decoder; std::string error;
    check(decoder.open(track, error), "corrupted source opens with intact metadata");
    std::array<StereoFrame, 1024> out; uint32_t good = 0; bool failed = false;
    for (uint32_t i = 0; i < 10; ++i) {
        uint32_t got = 99;
        if (!decoder.read_loop(out, got, error)) {
            check(good > 0 && !got && !decoder.valid() && !error.empty(), "actual short/decode failure closes source and discards complete output block");
            failed = true; break;
        }
        check(got > 0 && decoder.valid() && error.empty(), "successful loop read has live source and nonempty block");
        good += got;
    }
    check(failed, "corruption is detected during read, not only open");
    decoder.close();
    bytes = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg"); track.encoded = bytes;
    uint32_t got = 0;
    check(decoder.open(track, error) && decoder.read_loop(out, got, error) && got == 1024 && error.empty(), "reopen recovers after actual decode failure");
}
}
int main() {
    pcm();
    ogg("tests/audio/fixtures/stereo-48000.ogg", 37, 1234);
    ogg("engine/audio/assets/src/sfx_ping.ogg", 1023, 2049);
    refusals(); read_failure();
    std::puts(vorbis_fixture::fails ? "audio-music-decoder: FAIL" : "audio-music-decoder: PASS exact intro and loop samples, PCM, mono/stereo Ogg, bounds and refusal");
    return vorbis_fixture::fails ? 1 : 0;
}
