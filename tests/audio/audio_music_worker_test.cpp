#include "music_worker.hpp"

#include <cstdio>
#include <memory>
#include <vector>

#include "music_worker_fixture.hpp"

namespace {
using namespace audio;
using music_worker_fixture::exact;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("audio-music-worker: FAIL %s\n", why); } }
void init() {
    {
        auto t = std::make_unique<MusicTransport>(); MusicWorker worker(*t); std::string error;
        std::vector<MusicTrack> catalogue(4097, music_worker_fixture::tracks()[0]);
        check(!worker.init(catalogue, error), "catalogue cap checked before allocation/decoding");
        catalogue.pop_back();
        for (uint32_t i = 0; i < catalogue.size(); ++i) catalogue[i].id = i;
        check(worker.init(catalogue, error), "maximum4096 tracks accepted");
        catalogue.clear(); catalogue.shrink_to_fit(); // metadata owner может умереть, PCM storage остаётся
        check(t->request(0, 4095, 1) && worker.pump() && t->live_decoders.load() == 1 && !t->failures.load(), "worker owns a copy of catalogue values");
    }
    auto t = std::make_unique<MusicTransport>(); MusicWorker worker(*t); std::string error;
    auto tracks = music_worker_fixture::tracks();
    check(!worker.init({}, error) && !error.empty(), "empty catalogue refuses");
    auto duplicate = tracks; duplicate[1].id = duplicate[0].id;
    check(!worker.init(duplicate, error), "duplicate full64bit ids refuse");
    auto invalid = tracks; invalid[0].frames = 1;
    check(!worker.init(invalid, error), "all sources validated before worker starts");
    check(worker.init(tracks, error) && error.empty() && !t->live_decoders.load(), "cold source validation releases its decoder");
    check(!worker.init(tracks, error), "live worker cannot reconfigure borrowed catalogue");
    check(!t->request(2, 0, 1) && !t->request(0, 0, 0), "invalid slot/generation refused");
    MusicLoad request; check(!t->requests.pop(request), "invalid requests do not fill queue");
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP; ++i) check(t->request(0, i + 1 == MUSIC_REQUEST_CAP ? 1 : 999, i + 1), "fill request queue");
    check(!t->request(0, 0, MUSIC_REQUEST_CAP + 1), "full queue refuses and caller retains pending request");
    check(worker.pump() && !t->failures.load() && t->live_decoders.load() == 1, "coalesce obsolete loads before opening or failing them");
    check(t->slots[0].ready.load() == 0, "first1024 frames are not a250ms prefill");
    for (uint32_t i = 1; i < 11; ++i) { worker.pump(); check(t->slots[0].ready.load() == 0, "11264 published frames still below prefill"); }
    worker.pump();
    check(t->slots[0].ready.load() == MUSIC_REQUEST_CAP && t->peak_decoders.load() == 1, "latest generation primes at least250ms");
    StereoFrame frame;
    check(t->slots[0].pcm.take(MUSIC_REQUEST_CAP, frame) && exact(frame, tracks[1].pcm[0]), "obsolete wishes never publish their PCM");
    t->closed.store(true); check(!worker.pump() && !t->live_decoders.load(), "shutdown closes decoder");
}
void fifo(uint32_t block) {
    auto t = std::make_unique<MusicTransport>(); MusicWorker worker(*t); std::string error;
    const auto tracks = music_worker_fixture::tracks();
    check(worker.init(tracks, error) && t->request(0, 0, 1) && t->request(1, 1, 2), "load both decoder slots");
    for (uint32_t i = 0; i < 64; ++i) worker.pump(); // full rings plus one staged partial block
    check(t->live_decoders.load() == 2 && t->peak_decoders.load() == 2 && t->slots[0].ready.load() == 1 && t->slots[1].ready.load() == 2, "both sources ready with at most two decoders");
    for (uint32_t cursor = 0; cursor < 33000;) {
        for (uint32_t i = 0; i < block && cursor < 33000; ++i, ++cursor) {
            StereoFrame a, b;
            check(t->slots[0].pcm.take(1, a) && exact(a, music_worker_fixture::at(tracks[0], cursor)), "slotA exact intro/loop through partial pushes");
            check(t->slots[1].pcm.take(2, b) && exact(b, music_worker_fixture::at(tracks[1], cursor)), "slotB independent exact intro/loop through partial pushes");
        }
        worker.pump();
    }
    // Новый RT expected tag отбрасывает full old ring. Первый new кадр сохраняется до ready.
    constexpr uint64_t NEW = (uint64_t{1} << 32) | 1;
    check(t->request(0, 1, NEW), "replace slotA with a new high64 generation");
    bool primer = false; StereoFrame first;
    for (uint32_t i = 0; i < 80 && t->slots[0].ready.load() != NEW; ++i) {
        worker.pump();
        if (!primer) primer = t->slots[0].pcm.take(NEW, first);
    }
    check(primer && exact(first, tracks[1].pcm[0]) && t->slots[0].ready.load() == NEW, "stale drain plus cached first frame avoids priming deadlock and lost intro");
    uint32_t published = primer ? 1 : 0; StereoFrame frame;
    while (t->slots[0].pcm.take(NEW, frame)) {
        check(exact(frame, music_worker_fixture::at(tracks[1], published)), "generation restarts source at exact frame0"); ++published;
    }
    check(published >= SAMPLE_RATE / 4, "ready counts only committed frames, including cached primer");
    check(t->request(0, 999, NEW + 1) && worker.pump() && t->slots[0].failed.load() == NEW + 1 &&
        t->slots[0].ready.load() == 0 && t->failures.load() == 1 && t->live_decoders.load() == 1, "failed load retires only its slot with matching failure tag");
    check(t->request(1, MUSIC_STOP_TRACK, NEW + 2) && worker.pump() && t->live_decoders.load() == 0 &&
        t->slots[1].ready.load() == NEW + 2, "stop command closes source without resetting PCM cursors");
    check(t->request(0, 0, NEW + 100) && t->request(1, 1, NEW + 101), "reload both decoders before shutdown");
    for (uint32_t i = 0; i < 64; ++i) worker.pump();
    check(t->live_decoders.load() == 2, "both decoders remain live behind full stale rings");
    for (uint32_t i = 0; i < MUSIC_REQUEST_CAP; ++i) check(t->request(i % 2, i % 2, NEW + 3 + i), "saturate requests before shutdown");
    t->closed.store(true);
    check(!worker.pump() && !worker.pump() && !t->live_decoders.load() && !t->request(0, 0, NEW + 100), "closed bypasses full queue and callback acknowledgement");
}
}
int main() {
    init();
    for (uint32_t block : {1u, 17u, 256u, 1024u}) fifo(block);
    std::puts(fails ? "audio-music-worker: FAIL" : "audio-music-worker: PASS two decoders, priming, partial FIFO, generations, queue bounds and shutdown");
    return fails ? 1 : 0;
}
