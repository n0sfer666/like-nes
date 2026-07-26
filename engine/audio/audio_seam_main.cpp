#include <unistd.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "../asset/asset_manager.hpp"
#include "../asset/hash.hpp"
#include "decoder.hpp"
#include "engine.hpp"
#include "mixer.hpp"
#ifdef AUDIO_HAVE_MINIAUDIO
#include "device.hpp"
#endif

// Шов asset→audio (спека #3, аналог шва asset→render спеки #5): бейкнутый vorbis-ассет через
// ассет-пайплайн #5 (mmap zero-copy) → реальный stb_vorbis-декод → mixer (SFX резидент + музыка
// через стрим-ring) → offline WAV + RMS-санити. --play → реальное устройство miniaudio (локально).

using namespace audio;

namespace {

uint64_t guid_of(const char* n) { return asset::fnv1a(n, std::strlen(n)); }

void write_wav(const char* path, const int16_t* data, uint32_t frames, uint32_t ch, uint32_t rate) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    uint32_t bytes = frames * ch * 2, brate = rate * ch * 2, chunk = 36 + bytes;
    uint16_t block = static_cast<uint16_t>(ch * 2), bits = 16, fmt = 1, c16 = static_cast<uint16_t>(ch);
    uint32_t sub1 = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&chunk, 4, 1, f); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); std::fwrite(&sub1, 4, 1, f); std::fwrite(&fmt, 2, 1, f);
    std::fwrite(&c16, 2, 1, f); std::fwrite(&rate, 4, 1, f); std::fwrite(&brate, 4, 1, f);
    std::fwrite(&block, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&bytes, 4, 1, f);
    std::fwrite(data, 2, frames * ch, f);
    std::fclose(f);
}

// Долить стрим-ring музыкой (loop) впереди playhead.
void feed_ring(SampleRing& ring, const DecodedPcm& mus, uint32_t& pos) {
    while (ring.writable() > 0 && mus.frames) {
        int16_t s = mus.samples[pos];
        ring.push(&s, 1);
        pos = (pos + 1) % mus.frames;
    }
}

int fail(const char* m) { std::fprintf(stderr, "[audio_seam] FAIL: %s\n", m); return 1; }

void render_cb(void* user, uint32_t frames, int16_t* out) {
    static_cast<Mixer*>(user)->mix(frames, out);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return fail("usage: audio_seam <bundle> [--play]");
    const bool play = argc >= 3 && std::strcmp(argv[2], "--play") == 0;

    asset::AssetManager am;
    if (!am.open(argv[1], 8u * 1024 * 1024, /*trusted=*/false)) return fail("open bundle");
    const uint64_t gsfx = guid_of("sfx_ping"), gmus = guid_of("music");
    am.request(gsfx); am.request(gmus);
    for (int i = 0; i < 100; ++i) { am.sync_point(); if (am.is_ready(gsfx) && am.is_ready(gmus)) break; }
    if (!am.is_ready(gsfx) || !am.is_ready(gmus)) return fail("assets not ready");

    asset::Loaded lsfx = am.get(gsfx), lmus = am.get(gmus);
    DecodedPcm psfx, pmus;
    if (!decode_vorbis(lsfx.data, lsfx.size, psfx)) return fail("decode sfx_ping.ogg");
    if (!decode_vorbis(lmus.data, lmus.size, pmus)) return fail("decode music.ogg");
    if (psfx.rate != SAMPLE_RATE || pmus.rate != SAMPLE_RATE) return fail("asset rate != 48000");
    std::printf("[audio_seam] decoded sfx=%u frames music=%u frames @%uHz (zero-copy mmap ogg)\n",
                psfx.frames, pmus.frames, psfx.rate);

    Mixer mix(Backend::Float);
    mix.register_source(gsfx, psfx.samples.data(), psfx.frames, nullptr); // резидент SFX
    SampleRing ring;
    ring.init(SAMPLE_RATE); // ~1s стрим-буфер музыки
    mix.register_source(gmus, nullptr, 0, &ring);                         // стрим музыка
    uint32_t feed_pos = 0;
    feed_ring(ring, pmus, feed_pos);

    AudioEngine eng(mix);
    eng.set_listener(fix32(), fix32(), 0);
    PlayParams m; m.bus = Bus::Music; m.gain = fix32::from_float(0.8); m.loop = true;
    eng.play(gmus, m, 0);
    const uint32_t pings[] = {5, 15, 25, 35};
    for (uint32_t k : pings) {
        PlayParams p; p.bus = Bus::Sfx; p.gain = fix32::from_int(1);
        p.x = fix32::from_int((k % 2) ? 4 : -4);
        eng.play(gsfx, p, static_cast<uint64_t>(k) * SAMPLES_PER_TICK);
    }

    if (play) {
#ifdef AUDIO_HAVE_MINIAUDIO
        std::atomic<bool> done{false};
        std::thread feeder([&] { while (!done.load()) { feed_ring(ring, pmus, feed_pos); usleep(2000); } });
        MiniaudioDevice dev;
        if (!dev.start(render_cb, &mix)) { done = true; feeder.join(); return fail("device start"); }
        std::printf("[audio_seam] playing 3s on device @%uHz...\n", dev.sample_rate());
        usleep(3'000'000);
        dev.stop();
        done = true; feeder.join();
#else
        return fail("built without miniaudio (--play unavailable)");
#endif
    } else {
        const uint32_t total = SAMPLE_RATE * 2; // 2 сек offline
        std::vector<int16_t> out(static_cast<size_t>(total) * OUT_CHANNELS, 0);
        for (uint32_t off = 0; off < total; off += 256) {
            uint32_t n = (total - off < 256) ? total - off : 256;
            feed_ring(ring, pmus, feed_pos);
            mix.mix(n, out.data() + static_cast<size_t>(off) * OUT_CHANNELS);
        }
        write_wav("audio_seam.wav", out.data(), total, OUT_CHANNELS, SAMPLE_RATE);
        uint64_t sum = 0;
        for (int16_t s : out) sum += static_cast<uint64_t>(static_cast<int64_t>(s) * s);
        double rms = out.empty() ? 0.0 : std::sqrt(static_cast<double>(sum) / out.size());
        uint64_t h = asset::fnv1a(out.data(), out.size() * sizeof(int16_t));
        std::printf("[audio_seam] offline 2s -> audio_seam.wav  rms=%.1f  mix_hash=0x%016llx\n",
                    rms, (unsigned long long)h);
        if (rms < 200.0) return fail("silent mix (seam broken)");
    }

    am.close();
    std::printf("[audio_seam] PASS asset->decode->mix seam (real vorbis, stream+resident)\n");
    return 0;
}
