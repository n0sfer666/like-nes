#include "music_worker.hpp"

#include <algorithm>

namespace audio {
MusicWorker::~MusicWorker() { for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) close_slot(i); }
bool MusicWorker::init(std::span<const MusicTrack> tracks, std::string& error) {
    error.clear();
    if (initialized_ || tracks.empty() || tracks.size() > 4096 || transport_.closed.load()) {
        error = "music worker needs a fresh transport and1..4096 tracks"; return false;
    }
    std::vector<uint64_t> ids;
    for (const auto& track : tracks) ids.push_back(track.id);
    std::sort(ids.begin(), ids.end());
    for (size_t i = 1; i < ids.size(); ++i) if (ids[i - 1] == ids[i]) { error = "duplicate music track id"; return false; }
    // До старта worker максимум один cold decoder, все bytes остаются заимствованными.
    MusicDecoder probe;
    for (const auto& track : tracks) if (!probe.open(track, error)) return false;
    tracks_.assign(tracks.begin(), tracks.end()); initialized_ = true; return true;
}
void MusicWorker::close_slot(uint32_t i) {
    auto& slot = slots_[i];
    slot.decoder.close();
    if (slot.active) transport_.live_decoders.fetch_sub(1, std::memory_order_relaxed);
    slot.active = false; slot.offset = slot.count = slot.primed = 0;
    transport_.slots[i].ready.store(0, std::memory_order_release);
}
void MusicWorker::fail_slot(uint32_t i, uint64_t generation) {
    close_slot(i);
    transport_.failures.fetch_add(1, std::memory_order_relaxed);
    transport_.slots[i].failed.store(generation, std::memory_order_release);
}
void MusicWorker::load(const MusicLoad& request) {
    auto& slot = slots_[request.slot];
    if (slot.generation == request.generation) return; // idempotent duplicate, generation не переиспользуется
    close_slot(request.slot); slot.generation = request.generation;
    auto& output = transport_.slots[request.slot];
    output.failed.store(0, std::memory_order_release);
    if (request.track == MUSIC_STOP_TRACK) { output.ready.store(request.generation, std::memory_order_release); return; }
    std::string error;
    if (request.track >= tracks_.size() || !slot.decoder.open(tracks_[request.track], error)) {
        fail_slot(request.slot, request.generation); return;
    }
    slot.active = true;
    const uint32_t live = transport_.live_decoders.fetch_add(1, std::memory_order_relaxed) + 1;
    if (live > transport_.peak_decoders.load(std::memory_order_relaxed)) transport_.peak_decoders.store(live, std::memory_order_relaxed);
}
}
