#include "audio.hpp"

#include <algorithm>

#ifdef AUDIO_HAVE_MINIAUDIO

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "../engine/asset/asset_manager.hpp"
#include "../engine/asset/hash.hpp"
#include "../engine/audio/decoder.hpp"
#include "../engine/audio/device.hpp"
#include "../engine/audio/engine.hpp"
#include "../engine/audio/mixer.hpp"

namespace game {
namespace {
uint64_t guid_of(const char* n) { return asset::fnv1a(n, std::strlen(n)); }
void render_cb(void* user, uint32_t frames, int16_t* out) {
    static_cast<audio::Mixer*>(user)->mix(frames, out);
}
void feed_ring(audio::SampleRing& ring, const audio::DecodedPcm& mus, uint32_t& pos) {
    while (ring.writable() > 0 && mus.frames) {
        int16_t s = mus.samples[pos];
        ring.push(&s, 1);
        pos = (pos + 1) % mus.frames;
    }
}
} // namespace

struct GameAudio::Impl {
    asset::AssetManager am;
    audio::DecodedPcm psfx, pmus;
    audio::Mixer mixer{audio::Backend::Float};
    audio::SampleRing ring;
    audio::AudioEngine eng{mixer};
    audio::MiniaudioDevice dev;
    std::thread feeder;
    std::atomic<bool> done{false};
    uint32_t feed_pos = 0;
    uint64_t sample_time = 0;
    uint64_t gsfx = 0;
    bool live = false;
    bool dropped_warned = false;
};

GameAudio::~GameAudio() { shutdown(); }

bool GameAudio::init(const std::string& bundle_path) {
    Impl* p = new Impl();
    impl_ = p;
    if (!p->am.open(bundle_path, 8u * 1024 * 1024, /*trusted=*/false)) { shutdown(); return false; }
    p->gsfx = guid_of("sfx_ping");
    const uint64_t gmus = guid_of("music");
    p->am.request(p->gsfx); p->am.request(gmus);
    for (int i = 0; i < 200; ++i) {
        p->am.sync_point();
        if (p->am.is_ready(p->gsfx) && p->am.is_ready(gmus)) break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    if (!p->am.is_ready(p->gsfx) || !p->am.is_ready(gmus)) { shutdown(); return false; }
    asset::Loaded ls = p->am.get(p->gsfx), lm = p->am.get(gmus);
    if (!audio::decode_vorbis(ls.data, ls.size, p->psfx) ||
        !audio::decode_vorbis(lm.data, lm.size, p->pmus)) { shutdown(); return false; }

    p->mixer.register_source(p->gsfx, p->psfx.samples.data(), p->psfx.frames, nullptr);
    p->ring.init(audio::SAMPLE_RATE);
    p->mixer.register_source(gmus, nullptr, 0, &p->ring);
    feed_ring(p->ring, p->pmus, p->feed_pos);

    audio::PlayParams m; m.bus = audio::Bus::Music; m.gain = fix32::from_float(0.7); m.loop = true;
    p->eng.play(gmus, m, 0);

    if (!p->dev.start(render_cb, &p->mixer)) { shutdown(); return false; }
    p->feeder = std::thread([p] {
        while (!p->done.load()) {
            feed_ring(p->ring, p->pmus, p->feed_pos);
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    });
    p->live = true;
    return true;
}

void GameAudio::on_events(const FxSink& sink) {
    Impl* p = impl_;
    if (!p || !p->live) return;
    p->dev.poll();
    p->sample_time = audio::resync_clock(p->sample_time + audio::SAMPLES_PER_TICK, p->mixer.cursor());
    for (const FxEvent& e : sink.events) {
        double g = 0;
        switch (e.kind) {
            case FX_EnemyDie: g = 0.7; break;
            case FX_BossHit:  g = 0.35; break;
            case FX_BossDie:  g = 1.0; break;
            case FX_PlayerHit: g = 1.0; break;
            default: continue;   // FX_Fire — без звука (слишком часто)
        }
        audio::PlayParams pp; pp.bus = audio::Bus::Sfx; pp.gain = fix32::from_float(g);
        pp.x = fix32::from_float(e.x / 480.0);
        if (p->eng.play(p->gsfx, pp, p->sample_time) == 0 && !p->dropped_warned) {
            std::fprintf(stderr, "[game] audio queue full - sound dropped\n");
            p->dropped_warned = true;
        }
    }
}

int GameAudio::step_volume(int delta) {
    volume_ = std::clamp(volume_ + delta, 0, 10);
    if (impl_ && impl_->live)
        impl_->eng.set_master_gain(fix32::from_raw(volume_ * 65536 / 10), impl_->sample_time);
    return volume_;
}

void GameAudio::shutdown() {
    Impl* p = impl_;
    if (!p) return;
    if (p->live) { p->dev.stop(); p->done = true; if (p->feeder.joinable()) p->feeder.join(); }
    p->am.close();
    delete p;
    impl_ = nullptr;
}

} // namespace game

#else  // без miniaudio (headless/CI) — no-op

namespace game {
struct GameAudio::Impl {};
GameAudio::~GameAudio() { shutdown(); }
bool GameAudio::init(const std::string&) { return false; }
void GameAudio::on_events(const FxSink&) {}
int GameAudio::step_volume(int delta) { return volume_ = std::clamp(volume_ + delta, 0, 10); }
// impl_ здесь всегда nullptr — но освобождение симметрично живой ветке, а не «поле, которого
// в этой сборке будто нет»: иначе -Wunused-private-field справедливо ругается на заголовок.
void GameAudio::shutdown() { delete impl_; impl_ = nullptr; }
} // namespace game

#endif
