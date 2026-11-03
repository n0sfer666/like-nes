#include "vorbis_fixture.hpp"

#include <array>
#include <atomic>
#include <thread>

#include "decoder.hpp"
#include "vorbis_reader.hpp"

int main() {
    const auto bytes = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    if (bytes.empty()) return 1;
    audio::DecodedPcm expected;
    if (!audio::decode_vorbis(bytes.data(), static_cast<uint32_t>(bytes.size()), expected)) return 1;
    std::atomic<bool> start{false}, ok{true};
    auto run = [&] {
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (uint32_t i = 0; i < 40; ++i) {
            audio::VorbisReader reader; std::string error;
            audio::VorbisInfo info; audio::DecodedPcm mono;
            std::array<int16_t, 62> block{}; uint32_t frames = 0;
            if (!reader.open(bytes, error) || !reader.seek(i * 13) || !reader.read(block, 2, frames) || frames != 31 ||
                !audio::inspect_vorbis(bytes, info, error) || info.channels != 2 || info.frames != 4800 ||
                !audio::decode_vorbis(bytes.data(), static_cast<uint32_t>(bytes.size()), mono) || mono.samples != expected.samples) ok.store(false);
        }
    };
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) thread = std::thread(run);
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();
    const bool pass = ok.load() && !vorbis_fixture::fails;
    std::puts(pass ? "audio-vorbis-threads: PASS concurrent open, seek, read, inspect and legacy decode" : "audio-vorbis-threads: FAIL");
    return pass ? 0 : 1;
}
