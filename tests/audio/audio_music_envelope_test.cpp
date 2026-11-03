#include "music_envelope.hpp"

#include <cstdio>

#include "hash.hpp"

namespace {
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("audio-music-envelope: FAIL %s\n", why); } }
constexpr bool constant_eval() {
    audio::MusicEnvelope envelope;
    envelope.ramp_to(fix32::ONE, 3); envelope.advance(3);
    return envelope.done() && envelope.value() == fix32::ONE;
}
static_assert(constant_eval());
void bounds() {
    using audio::MusicEnvelope;
    MusicEnvelope env;
    check(env.value() == 0 && env.done(), "default silence");
    env.ramp_to(INT32_MAX, 0); check(env.value() == fix32::ONE && env.done(), "zero-length instant clamp");
    env.advance(UINT32_MAX); check(env.value() == fix32::ONE, "constant gain cannot overflow time");
    env.ramp_to(INT32_MIN, 1); check(env.value() == fix32::ONE && !env.done(), "one frame starts at current boundary value");
    env.advance(); check(env.done() && !env.value(), "one frame exact endpoint");
    env.ramp_to(fix32::ONE, 3); env.advance(); check(env.value() == 21845, "Q16 rational third");
    env.hold(); env.advance(UINT32_MAX); check(env.done() && env.value() == 21845, "hold freezes the current boundary gain");
    env.ramp_to(0, 3); env.advance(); check(env.value() == 14564, "negative interpolation truncates symmetrically to zero");
    env.reset(-1); check(!env.value() && env.done(), "reset clamps and ends old ramp");
    MusicEnvelope up, down; down.reset(fix32::ONE);
    up.ramp_to(fix32::ONE, UINT32_MAX); down.ramp_to(0, UINT32_MAX);
    up.advance(uint32_t{1} << 31); down.advance(uint32_t{1} << 31);
    check(up.value() == 32768 && down.value() == 32768, "maximum duration multiplication remains64bit bounded");
    up.advance(UINT32_MAX); down.advance(UINT32_MAX);
    check(up.done() && down.done() && up.value() == fix32::ONE && !down.value(), "maximum advance saturates without counter wrap");
    for (uint32_t n : {0u, 1u, 3u, 7u, 480u, 48000u}) {
        up.reset(0); down.reset(fix32::ONE); up.ramp_to(fix32::ONE, n); down.ramp_to(0, n);
        for (uint32_t t = 0; t < n + 2; ++t) {
            check(up.value() + down.value() == fix32::ONE, "complementary ramps partition unity at every sample");
            up.advance(); down.advance();
        }
    }
    for (uint32_t count = 0; count < 40; ++count) {
        MusicEnvelope a, b; a.reset(12345); b.reset(12345); a.ramp_to(60001, 17); b.ramp_to(60001, 17);
        a.advance(count); for (uint32_t i = 0; i < count; ++i) b.advance();
        check(a.value() == b.value() && a.done() == b.done(), "advance independent of callback block size");
    }
}
uint64_t trace() {
    uint64_t hash = asset::FNV_OFFSET;
    auto add = [&](const audio::MusicEnvelope& e) { hash = asset::fnv1a_u32(hash, static_cast<uint32_t>(e.value())); };
    audio::MusicEnvelope up, down; down.reset(fix32::ONE);
    up.ramp_to(fix32::ONE, 7); down.ramp_to(0, 7);
    for (uint32_t t = 0; t < 10; ++t) { add(down); add(up); up.advance(); down.advance(); }
    audio::MusicEnvelope changed; changed.reset(12345); changed.ramp_to(60001, 31);
    for (uint32_t t = 0; t < 80; ++t) {
        if (t == 9) changed.hold();
        if (t == 15) changed.ramp_to(99, 480);
        if (t == 37) changed.ramp_to(fix32::ONE, 3);
        add(changed); changed.advance();
    }
    return hash;
}
}
int main() {
    bounds(); const auto hash = trace();
    std::printf("audio-music-envelope: gain hash=%016llx\n", static_cast<unsigned long long>(hash));
    check(hash == 0x67cb1cc48fe494dbull, "independent Python Fraction+LE oracle for100 gain values");
    std::puts(fails ? "audio-music-envelope: FAIL" : "audio-music-envelope: PASS linear samples, hold, retarget, endpoints, clamps and bounded arithmetic");
    return fails ? 1 : 0;
}
