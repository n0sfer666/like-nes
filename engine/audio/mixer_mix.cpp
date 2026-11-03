#include "mixer.hpp"

#include <cmath>

namespace audio {
namespace {
const fix32 ONE = fix32::from_int(1);
const fix32 DUCK_GAIN = fix32::from_float(0.35);
const fix32 DUCK_STEP = fix32::from_float(0.0003);
constexpr uint32_t BUS_N = static_cast<uint32_t>(Bus::Count);
fix32 approach(fix32 cur, fix32 target, fix32 step) {
    if (cur.raw < target.raw) { fix32 n = cur + step; return n.raw > target.raw ? target : n; }
    fix32 n = cur - step;
    return n.raw < target.raw ? target : n;
}

// Одно продвижение на output frame. Моно дублируется до старого pan/attenuation;
// стерео сохраняет каналы. Streaming mono underrun не деактивирует голос.
StereoFrame fetch_advance(Voice& v, uint64_t& underruns) {
    if (v.ring) {
        if (v.ring->readable() == 0) { ++underruns; return {}; }
        const auto sample = v.ring->pop_one();
        return {sample, sample};
    }
    if (v.frames == 0) { v.active = false; return {}; }
    if (v.playhead >= v.frames) {
        if (v.loop) v.playhead = 0;
        else { v.active = false; return {}; }
    }
    if (v.stereo) return v.stereo_pcm[v.playhead++];
    const auto sample = v.pcm[v.playhead++];
    return {sample, sample};
}
} // namespace

void Mixer::mix_fix(uint32_t frames, int16_t* out) {
    for (uint32_t f = 0; f < frames; ++f) {
        int64_t busL[BUS_N] = {0}, busR[BUS_N] = {0};
        bool duck_now = false;
        for (uint32_t i = 0; i < pool_.size(); ++i) {
            Voice& v = pool_[i];
            if (!v.active || f < v.start_offset) continue; // ещё не звучит в этом сэмпле
            if (v.ducking) duck_now = true;
            const auto sample = fetch_advance(v, underruns_);
            busL[static_cast<uint32_t>(v.bus)] += int64_t{sample.left} * v.gl.raw;
            busR[static_cast<uint32_t>(v.bus)] += int64_t{sample.right} * v.gr.raw;
        }
        duck_env_ = approach(duck_env_, duck_now ? DUCK_GAIN : ONE, DUCK_STEP); // per-sample рамп
        int64_t mL = 0, mR = 0;
        for (uint32_t b = 0; b < BUS_N; ++b) {
            fix32 bg = bus_gain_[b];
            if (b == static_cast<uint32_t>(Bus::Music)) bg = bg * duck_env_;
            mL += (busL[b] * bg.raw) >> 16;
            mR += (busR[b] * bg.raw) >> 16;
        }
        limiter_.apply((mL * master_.raw) >> 32, (mR * master_.raw) >> 32, out + f * 2);
    }
}

void Mixer::mix_float(uint32_t frames, int16_t* out) {
    for (uint32_t f = 0; f < frames; ++f) {
        double busL[BUS_N] = {0}, busR[BUS_N] = {0};
        bool duck_now = false;
        for (uint32_t i = 0; i < pool_.size(); ++i) {
            Voice& v = pool_[i];
            if (!v.active || f < v.start_offset) continue;
            if (v.ducking) duck_now = true;
            const auto sample = fetch_advance(v, underruns_);
            busL[static_cast<uint32_t>(v.bus)] += sample.left * v.gl.to_double();
            busR[static_cast<uint32_t>(v.bus)] += sample.right * v.gr.to_double();
        }
        duck_env_ = approach(duck_env_, duck_now ? DUCK_GAIN : ONE, DUCK_STEP); // per-sample рамп
        double mL = 0, mR = 0;
        for (uint32_t b = 0; b < BUS_N; ++b) {
            double bg = bus_gain_[b].to_double();
            if (b == static_cast<uint32_t>(Bus::Music)) bg *= duck_env_.to_double();
            mL += busL[b] * bg;
            mR += busR[b] * bg;
        }
        limiter_.apply(std::llround(mL * master_.to_double()), std::llround(mR * master_.to_double()),
                       out + f * 2);
    }
}

} // namespace audio
