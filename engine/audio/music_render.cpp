#include "music_runtime.hpp"

namespace audio {
MusicMix MusicRuntime::next() {
    MusicMix result;
    if (!valid_ || transport_.closed.load(std::memory_order_acquire)) return result;
    update();
    if (transport_.closed.load(std::memory_order_acquire)) return result;
    for (uint32_t i = 0; i < MUSIC_SLOTS; ++i) {
        Slot& slot = slots_[i];
        if (slot.phase != Phase::Playing || transport_.closed.load(std::memory_order_acquire)) continue;
        StereoFrame frame;
        bool available = false;
        if (slot.has_primer) {
            frame = slot.primer;
            slot.has_primer = false;
            available = true;
        } else {
            available = transport_.slots[i].pcm.take(slot.generation, frame);
        }
        if (!available) {
            ++result.underruns;
        } else {
            const int32_t gain = slot.gain.value();
            result.left += static_cast<int64_t>(frame.left) * gain;
            result.right += static_cast<int64_t>(frame.right) * gain;
        }
        slot.gain.advance();
    }
    return result;
}
} // namespace audio
