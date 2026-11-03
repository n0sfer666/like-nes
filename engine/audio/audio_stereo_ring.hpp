#pragma once
#include <span>

#include "audio_spsc.hpp"
#include "audio_types.hpp"

namespace audio {
// Startup-owned storage: один decode-worker пишет, один RT-consumer читает, оба заканчивают
// работу до уничтожения. Поколение выбирает consumer; worker получает его отдельной командой.
// Никаких reset курсоров: запоздалые блоки старого worker отбрасываются по тегу поколения.
class StereoRing {
public:
    static constexpr uint32_t CAPACITY = 16384; //341ms при48000Hz, минимум250ms
    static_assert(std::atomic<size_t>::is_always_lock_free);
    // Только producer. Частичная запись; за вызов не более CAPACITY кадров. Поколение0 невалидно.
    uint32_t push(uint64_t generation, std::span<const StereoFrame> frames) {
        if (!generation) return 0;
        const size_t count = frames.size() < CAPACITY ? frames.size() : CAPACITY;
        uint32_t written = 0;
        while (written < count && queue_.push({generation, frames[written]})) ++written;
        return written;
    }
    // Только consumer. false/нулевой out = underrun, вызывающий считает его сам.
    // Scan ограничен CAPACITY даже если worker одновременно дописывает stale packets.
    bool take(uint64_t generation, StereoFrame& out) {
        out = {};
        if (!generation) return false;
        Packet packet;
        for (uint32_t scanned = 0; scanned < CAPACITY && queue_.pop(packet); ++scanned) {
            if (packet.generation == generation) { out = packet.frame; return true; }
        }
        return false;
    }
private:
    struct Packet { uint64_t generation = 0; StereoFrame frame; };
    SpscQueue<Packet, CAPACITY> queue_;
};
}
