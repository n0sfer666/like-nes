#pragma once
#include <array>

#include "audio_stereo_ring.hpp"

namespace audio {
inline constexpr uint32_t MUSIC_SLOTS = 2, MUSIC_REQUEST_CAP = 16, MUSIC_PREFILL = SAMPLE_RATE / 4;
inline constexpr uint32_t MUSIC_STOP_TRACK = UINT32_MAX;
struct MusicLoad { uint64_t generation = 0; uint32_t slot = 0, track = 0; };
struct MusicBuffer {
    StereoRing pcm;
    // ready: ≥PREFILL суммарно опубликованных кадров, а не текущая occupancy; STOP — только ack.
    // При priming RT кэширует максимум первый matching frame и дальше ждёт ready своего tag.
    // Worker пишет, RT читает. Данные кольца опубликованы до ready; совпасть должен весь tag.
    std::atomic<uint64_t> ready{0}, failed{0};
};
// Startup-owned до stop/join producer и consumer. Callback не владеет refcount этого storage.
// Один RT producer запросов, один worker consumer/producer PCM, один RT consumer PCM.
struct MusicTransport {
    static_assert(std::atomic<uint64_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    std::array<MusicBuffer, MUSIC_SLOTS> slots;
    SpscQueue<MusicLoad, MUSIC_REQUEST_CAP> requests;
    std::atomic<bool> closed{false}; // shutdown обходит полные очереди и paused callback
    std::atomic<uint32_t> live_decoders{0}, peak_decoders{0};
    std::atomic<uint64_t> failures{0};
    // RT сначала устанавливает expected generation, затем публикует этот запрос.
    // false при full: caller сохраняет pending request и повторяет, не теряя последнее желание.
    bool request(uint32_t slot, uint32_t track, uint64_t generation) {
        if (slot >= MUSIC_SLOTS || !generation || closed.load(std::memory_order_acquire)) return false;
        return requests.push({generation, slot, track});
    }
};
}
