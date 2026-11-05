#include "rumble_frame_music.hpp"

#if defined(AUDIO_HAVE_MINIAUDIO)
#include <algorithm>
#include <atomic>

#include "bundle_lookup.hpp"
#include "bundle_view.hpp"
#include "device.hpp"
#include "hash.hpp"
#include "mixer.hpp"
#include "music_read.hpp"
#include "music_stream.hpp"
#include "platform_io.hpp"
#include "rollback_timing.hpp"
#endif

namespace rumble {

#if defined(AUDIO_HAVE_MINIAUDIO)
namespace {

bool track_from_bundle(const asset::BundleView& bundle, audio::MusicTrack& track) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    framework::music::MusicTable table;
    if (asset::raw_table(bundle, asset::fnv1a_str("music"), data, size) != asset::LookupFault::Ok ||
        !table.open(data, size)) return false;
    const auto* row = table.find("frame_stats");
    if (!row) return false;
    const auto* entry = bundle.find(row->guid);
    if (!entry || entry->type != static_cast<uint32_t>(asset::AssetType::Audio) ||
        entry->codec != static_cast<uint32_t>(asset::Codec::Vorbis) ||
        entry->tex_w != row->rate || entry->tex_h != row->channels || entry->tex_format != row->frames)
        return false;
    track = {row->id, {bundle.payload(*entry), static_cast<size_t>(entry->payload_size)}, {},
             row->frames, row->loop_start, row->loop_end, row->fade_samples};
    return true;
}

} // namespace

struct FrameMusic::Impl {
    static void render(void* user, uint32_t frames, int16_t* out) {
        auto& self = *static_cast<Impl*>(user);
        self.mixer.mix(frames, out);
        const auto samples = static_cast<size_t>(frames) * audio::OUT_CHANNELS;
        if (std::any_of(out, out + samples, [](int16_t v) { return v != 0; }))
            self.audible.fetch_add(1, std::memory_order_relaxed);
    }
    bool start() { return device.start(render, this, null_audio); }

    platform::MappedFile file;
    audio::Mixer mixer{audio::Backend::Float};
    audio::MusicStream stream;
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    std::atomic<uint64_t> audible{0};
    uint64_t track = 0;
    uint32_t ticks = 0;
    bool null_audio = false, failed = false, paused = false;
    // Stops/joins callbacks before their user or any encoded bytes are destroyed.
    audio::MiniaudioDevice device;
};

FrameMusic::FrameMusic() = default;
FrameMusic::~FrameMusic() = default;

bool FrameMusic::open(const std::string& path, bool null_audio, std::string& why) {
    if (impl_) return false;
    auto music = std::make_unique<Impl>();
    asset::BundleView bundle;
    audio::MusicTrack track;
    if (!music->file.open(path) || !bundle.open(music->file.data(), music->file.size(), false) ||
        !track_from_bundle(bundle, track)) {
        why = "cannot load the benchmark music bundle";
        return false;
    }
    if (!music->stream.init(music->mixer, {&track, 1}, framework::rollback::SETTLE_TICKS<8>, why))
        return false;
    music->track = track.id;
    music->null_audio = null_audio;
    if (!music->start()) {
        why = "cannot start the benchmark audio device";
        return false;
    }
    impl_ = std::move(music);
    return true;
}

void FrameMusic::tick() {
    if (!impl_ || impl_->paused || impl_->failed) return;
    impl_->stream.want(impl_->track);
    impl_->stream.tick(++impl_->ticks);
}

void FrameMusic::poll() {
    if (impl_ && !impl_->paused && impl_->device.poll() == audio::DeviceEvent::Failed) impl_->failed = true;
}

void FrameMusic::suspend() {
    if (!impl_ || impl_->paused) return;
    impl_->device.stop();
    impl_->paused = true;
}

void FrameMusic::resume() {
    if (!impl_ || !impl_->paused || impl_->failed) return;
    impl_->failed = !impl_->start();
    impl_->paused = false;
}

FrameMusicStats FrameMusic::stats() const {
    if (!impl_) return {};
    const auto stats = impl_->stream.stats();
    return {stats.underruns, impl_->audible.load(std::memory_order_relaxed),
            !impl_->failed && !stats.closed && stats.decode_failures == 0};
}
#else
struct FrameMusic::Impl {};
FrameMusic::FrameMusic() = default;
FrameMusic::~FrameMusic() = default;

bool FrameMusic::open(const std::string&, bool, std::string& why) {
    why = "--frame-stats requires an SDK built with AUDIO_MINIAUDIO=ON";
    return false;
}
void FrameMusic::tick() {}
void FrameMusic::poll() {}
void FrameMusic::suspend() {}
void FrameMusic::resume() {}
FrameMusicStats FrameMusic::stats() const { return {}; }
#endif

} // namespace rumble
