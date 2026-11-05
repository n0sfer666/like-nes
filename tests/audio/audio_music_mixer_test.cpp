#include "music_stream_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "hash.hpp"
#include "audio_engine.hpp"

namespace {
struct Event { uint32_t frame, track, fade; };
constexpr std::array<Event, 5> EVENTS{{{0, 0, 100}, {200, 1, 1000}, {400, 2, 500}, {450, 3, 600}, {1000, 1, 1000}}};
constexpr std::array<uint32_t, 8> CUTS{1, 200, 201, 400, 450, 881, 1000, 2300};
struct Render { std::vector<int16_t> samples; uint64_t hash = 0; bool ok = false; };

Render render(audio::Backend backend, uint32_t block_size, int32_t gain) {
    using namespace audio;
    music_stream_fixture::ManualRig rig(backend);
    Render result;
    if (!rig.ready || !rig.bound) return result;
    AudioEngine engine(*rig.mixer);
    engine.set_bus_gain(Bus::Music, fix32::from_raw(gain), 0);
    engine.set_master_gain(fix32::from_raw(gain), 0);
    result.samples.reserve(4600);
    std::array<int16_t, 800 * 2> output{};
    uint32_t frame = 0, event = 0, cut = 0;
    while (frame < CUTS.back()) {
        if (event < EVENTS.size() && frame == EVENTS[event].frame) {
            if (!rig.runtime->post({EVENTS[event].track, EVENTS[event].fade})) return result;
            ++event;
        }
        while (cut < CUTS.size() && CUTS[cut] <= frame) ++cut;
        if (cut == CUTS.size()) break;
        const uint32_t count = std::min({block_size, CUTS[cut] - frame, CUTS.back() - frame});
        rig.mixer->mix(count, output.data());
        result.samples.insert(result.samples.end(), output.begin(), output.begin() + count * 2);
        frame += count;
        rig.pump();
    }
    result.hash = asset::FNV_OFFSET;
    for (size_t i = 0; i < result.samples.size(); i += 2) {
        const uint32_t word = static_cast<uint16_t>(result.samples[i]) |
            (static_cast<uint32_t>(static_cast<uint16_t>(result.samples[i + 1])) << 16);
        result.hash = asset::fnv1a_u32(result.hash, word);
    }
    result.ok = event == EVENTS.size() && result.samples.size() == CUTS.back() * 2 &&
        rig.runtime->stats().underruns == 0 && rig.mixer->underruns() == 0 && rig.mixer->peak_voices() == 0;
    return result;
}
}

int main() {
    int fails = 0;
    std::array<uint64_t, 2> hashes{};
    for (uint32_t mode = 0; mode < 2; ++mode) {
        const int32_t gain = mode ? 49152 : fix32::ONE;
        const uint64_t expected = mode ? 0x94b29cb8987f09c3ull : 0xc3667d5c010ee2c0ull;
        const Render fixed = render(audio::Backend::Fix32, 480, gain);
        hashes[mode] = fixed.hash;
        if (!fixed.ok || fixed.hash != expected) ++fails;
        for (uint32_t block : {1u, 17u, 47u, 480u, 800u}) {
            const Render f = render(audio::Backend::Fix32, block, gain);
            const Render floating = render(audio::Backend::Float, block, gain);
            bool close = f.ok && floating.ok && f.samples.size() == floating.samples.size();
            for (size_t i = 0; close && i < f.samples.size(); ++i)
                close = std::abs(static_cast<int32_t>(f.samples[i]) - floating.samples[i]) <= 1;
            if (f.hash != fixed.hash || !close) ++fails;
        }
    }
    std::printf("audio-music-mixer: golden=%016llx\n", static_cast<unsigned long long>(hashes[0]));
    std::printf("audio-music-mixer: scaled-golden=%016llx\n", static_cast<unsigned long long>(hashes[1]));
    std::puts(fails ? "audio-music-mixer: FAIL independent delay47 oracle, Float parity and block cuts" :
        "audio-music-mixer: PASS independent delay47 oracle, Float parity and block cuts");
    return fails ? 1 : 0;
}
