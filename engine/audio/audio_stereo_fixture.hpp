#pragma once
#include <algorithm>
#include <array>
#include <vector>

#include "audio_engine.hpp"

namespace audio_stereo_fixture {
inline constexpr std::array<audio::StereoFrame, 5> PCM{{{800, -1600}, {-2400, 3200}, {4000, 0}, {0, -4800}, {5600, 6400}}};
inline std::vector<int16_t> render(audio::Backend backend, uint32_t block = 256, bool loop = true,
                                  fix32 gain = fix32::from_int(1), fix32 bus = fix32::from_int(1),
                                  fix32 master = fix32::from_int(1), uint64_t start = 0) {
    audio::Mixer mix(backend);
    if (!mix.register_stereo_source(1, PCM.data(), static_cast<uint32_t>(PCM.size()))) return {};
    audio::AudioEngine engine(mix);
    engine.set_listener(fix32::from_int(20), fix32::from_int(20), 0);
    engine.set_bus_gain(audio::Bus::Sfx, bus, 0); engine.set_master_gain(master, 0);
    audio::PlayParams params; params.loop = loop; params.gain = gain;
    params.x = params.y = fix32::from_int(-4); // Моно на таком расстоянии молчит; стерео не spatial.
    if (!engine.play(1, params, start)) return {};
    std::vector<int16_t> output(512);
    for (uint32_t at = 0; at < 256; at += block) mix.mix(std::min(block, 256 - at), output.data() + at * 2);
    return output;
}
}
