#include "music_worker.hpp"

#include <algorithm>
#include <optional>

namespace audio {
bool MusicWorker::pump() {
    if (!initialized_ || transport_.closed.load(std::memory_order_acquire)) {
        for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) close_slot(i);
        return false;
    }
    // Не открываем вытесненные команды. Даже при непрерывном producer drain ограничен CAP.
    std::array<std::optional<MusicLoad>, MUSIC_SLOTS> latest;
    MusicLoad request;
    for (uint32_t count = 0; count < MUSIC_REQUEST_CAP && transport_.requests.pop(request); ++count) {
        if (request.slot >= MUSIC_SLOTS || !request.generation) { transport_.failures.fetch_add(1, std::memory_order_relaxed); continue; }
        latest[request.slot] = request;
    }
    for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) {
        if (transport_.closed.load(std::memory_order_acquire)) {
            for (uint32_t j = 0; j < MUSIC_SLOTS; ++j) close_slot(j);
            return false;
        }
        if (latest[i]) load(*latest[i]);
        auto& slot = slots_[i];
        if (!slot.active) continue;
        if (slot.offset == slot.count) {
            std::string error;
            if (!slot.decoder.read_loop(slot.block, slot.count, error) || !slot.count || slot.count > slot.block.size()) {
                fail_slot(i, slot.generation); continue;
            }
            slot.offset = 0;
        }
        auto& buffer = transport_.slots[i];
        const auto frames = std::span(slot.block).subspan(slot.offset, slot.count - slot.offset);
        const auto written = buffer.pcm.push(slot.generation, frames);
        slot.offset += written;
        slot.primed = std::min(MUSIC_PREFILL, slot.primed + written);
        if (slot.primed == MUSIC_PREFILL) buffer.ready.store(slot.generation, std::memory_order_release);
    }
    return true;
}
}
