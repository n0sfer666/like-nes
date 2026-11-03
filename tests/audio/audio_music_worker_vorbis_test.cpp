#include "music_worker.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "decoder.hpp"
#include "music_worker_fixture.hpp"
#include "music_ogg_corruption.hpp"
#include "vorbis_fixture.hpp"

namespace {
std::vector<audio::StereoFrame> full(std::span<const uint8_t> bytes) {
    audio::VorbisReader reader; std::string error;
    if (!reader.open(bytes, error)) return {};
    std::vector<int16_t> raw(static_cast<size_t>(reader.info().frames) * 2); uint32_t got = 0;
    if (!reader.read(raw, 2, got) || got != reader.info().frames) return {};
    std::vector<audio::StereoFrame> result(got);
    for (uint32_t i = 0; i < got; ++i) result[i] = {raw[i * 2], raw[i * 2 + 1]};
    return result;
}
audio::StereoFrame at(std::span<const audio::StereoFrame> source, const audio::MusicTrack& track, uint32_t frame) {
    return source[frame < track.loop_end ? frame : track.loop_start + (frame - track.loop_end) % (track.loop_end - track.loop_start)];
}
bool read_failure(const std::vector<uint8_t>& source, audio::StereoFrame first) {
    using namespace audio;
    auto corrupted = source;
    if (!music_ogg_corruption::apply(corrupted)) return false;
    std::array<MusicTrack, 2> tracks;
    tracks[0].id = 1; tracks[0].encoded = corrupted; tracks[0].frames = tracks[0].loop_end = 4800;
    tracks[1].id = 2; tracks[1].encoded = source; tracks[1].frames = tracks[1].loop_end = 4800;
    auto t = std::make_unique<MusicTransport>(); MusicWorker worker(*t); std::string error;
    constexpr uint64_t BAD = (uint64_t{1} << 48) | 1;
    if (!worker.init(tracks, error) || !t->request(0, 0, BAD) || !t->request(1, 1, 2)) return false;
    for (uint32_t i = 0; i < 20; ++i) worker.pump();
    bool ok = t->slots[0].failed.load() == BAD && !t->slots[0].ready.load() && t->live_decoders.load() == 1 && t->failures.load() == 1 &&
        t->slots[1].ready.load() == 2;
    StereoFrame frame; uint32_t published = 0;
    while (t->slots[0].pcm.take(BAD, frame)) ++published;
    ok &= published == 4096; // ошибочный пятый block не попал в ring даже частично
    ok &= t->slots[1].pcm.take(2, frame) && music_worker_fixture::exact(frame, first);
    ok &= t->request(0, 1, BAD + 1);
    for (uint32_t i = 0; i < 20; ++i) worker.pump();
    ok &= t->slots[0].ready.load() == BAD + 1 && !t->slots[0].failed.load() && t->live_decoders.load() == 2 &&
        t->peak_decoders.load() == 2 && t->slots[0].pcm.take(BAD + 1, frame) && music_worker_fixture::exact(frame, first);
    // Duplicate того же tag не перезапускает intro и не очищает уже накопленный ready.
    ok &= t->request(0, 1, BAD + 1) && worker.pump() && t->slots[0].ready.load() == BAD + 1 && t->live_decoders.load() == 2;
    t->closed.store(true); ok &= !worker.pump() && !t->live_decoders.load();
    return ok;
}
}
int main() {
    using namespace audio;
    const auto a = vorbis_fixture::load("tests/audio/fixtures/stereo-48000.ogg");
    const auto b = vorbis_fixture::load("engine/audio/assets/src/sfx_ping.ogg");
    const auto reference_a = full(a), reference_b = full(b);
    if (reference_a.size() != 4800 || reference_b.size() != 14400 || vorbis_fixture::fails) return 1;
    if (!read_failure(a, reference_a[0])) { std::puts("audio-music-worker-vorbis: FAIL decode failure publication/recovery"); return 1; }
    std::array<MusicTrack, 2> tracks;
    tracks[0].id = 1; tracks[0].encoded = a; tracks[0].frames = 4800; tracks[0].loop_start = 37; tracks[0].loop_end = 1234;
    tracks[1].id = 2; tracks[1].encoded = b; tracks[1].frames = 14400; tracks[1].loop_start = 1023; tracks[1].loop_end = 2049;
    auto t = std::make_unique<MusicTransport>(); MusicWorker worker(*t); std::string error;
    if (!worker.init(tracks, error) || !t->request(0, 0, 1) || !t->request(1, 1, 2)) return 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    std::atomic<bool> failed{false};
    auto expired = [&] { return std::chrono::steady_clock::now() > deadline; };
    std::thread decoder([&] {
        while (worker.pump()) {
            if (expired()) { failed.store(true); t->closed.store(true); break; }
            std::this_thread::yield();
        }
    });
    // Legacy SFX decoder открывает свои contexts параллельно seek/loop музыки: общий CRC pin.
    std::atomic<uint32_t> effects{0};
    std::thread effect([&] {
        while (!t->closed.load()) {
            DecodedPcm pcm;
            if (!decode_vorbis(a.data(), static_cast<uint32_t>(a.size()), pcm) || pcm.frames != 4800 || pcm.samples.size() != 4800 ||
                pcm.samples[31] != (int32_t{reference_a[31].left} + reference_a[31].right) / 2) failed.store(true);
            effects.fetch_add(1); std::this_thread::yield();
        }
    });
    while (!effects.load() && !failed.load() && !expired()) std::this_thread::yield();
    std::array<bool, 2> primed{}; std::array<StereoFrame, 2> first;
    while ((t->slots[0].ready.load() != 1 || t->slots[1].ready.load() != 2) && !failed.load() && !expired()) {
        for (uint32_t i = 0; i < 2; ++i) if (!primed[i]) primed[i] = t->slots[i].pcm.take(i + 1, first[i]);
        std::this_thread::yield();
    }
    bool ok = !failed.load() && !expired();
    std::array<uint32_t, 2> cursor{};
    const std::array<std::span<const StereoFrame>, 2> reference{reference_a, reference_b};
    for (uint32_t i = 0; i < 2; ++i) if (primed[i]) {
        ok &= music_worker_fixture::exact(first[i], reference[i][0]); cursor[i] = 1;
    }
    while ((cursor[0] < 33000 || cursor[1] < 33000) && ok) {
        for (uint32_t i = 0; i < 2; ++i) if (cursor[i] < 33000) {
            StereoFrame frame;
            if (t->slots[i].pcm.take(i + 1, frame)) {
                ok &= music_worker_fixture::exact(frame, at(reference[i], tracks[i], cursor[i])); ++cursor[i];
            }
        }
        if (failed.load() || expired()) ok = false;
    }
    t->closed.store(true); decoder.join(); effect.join();
    ok &= !failed.load() && effects.load() > 0 && !t->failures.load() && !t->live_decoders.load() && t->peak_decoders.load() == 2;
    std::puts(ok ? "audio-music-worker-vorbis: PASS concurrent mono/stereo loops, exact samples and legacy SFX CRC safety" : "audio-music-worker-vorbis: FAIL");
    return ok ? 0 : 1;
}
