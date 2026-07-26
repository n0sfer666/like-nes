#pragma once
#include <cstdint>

#include "audio_types.hpp"
#include "spsc.hpp"

// Голос микшера. Источник — резидентный PCM (int16 mono, playhead) ИЛИ стрим-ring.
// Пул фиксированный (MAX_VOICES) → без per-callback heap. Переполнение = voice-stealing
// (детерм.: min-priority, затем меньший id).
namespace audio {

struct Voice {
    bool active = false;
    uint32_t id = 0;
    uint64_t start_seq = 0;         // порядок запуска (детерм. tie-break для steal)
    const int16_t* pcm = nullptr;   // резидент
    uint32_t frames = 0;
    uint32_t playhead = 0;
    SampleRing* ring = nullptr;     // стрим (если != nullptr — streaming)
    fix32 gain;
    fix32 x, y;                     // мировая позиция (для пана/аттенюации)
    Bus bus = Bus::Sfx;
    bool loop = false;
    bool ducking = false;
    uint8_t priority = 128;
    uint32_t start_offset = 0;      // intra-block старт (sample-accurate в пределах блока)
    // Предрасчёт на границе блока (control — всегда fix32, детерм.):
    fix32 gl, gr;                   // итоговые L/R gain голоса (gain*atten*pan)
};

class VoicePool {
public:
    Voice& operator[](uint32_t i) { return v_[i]; }
    uint32_t size() const { return MAX_VOICES; }

    // Свободный слот или кража (min-priority, tie → меньший start_seq).
    Voice& alloc(uint64_t seq) {
        for (uint32_t i = 0; i < MAX_VOICES; ++i)
            if (!v_[i].active) { reset(v_[i], seq); return v_[i]; }
        uint32_t victim = 0;
        for (uint32_t i = 1; i < MAX_VOICES; ++i) {
            if (v_[i].priority < v_[victim].priority ||
                (v_[i].priority == v_[victim].priority && v_[i].start_seq < v_[victim].start_seq))
                victim = i;
        }
        reset(v_[victim], seq);
        return v_[victim];
    }

    Voice* find(uint32_t id) {
        for (uint32_t i = 0; i < MAX_VOICES; ++i)
            if (v_[i].active && v_[i].id == id) return &v_[i];
        return nullptr;
    }

private:
    static void reset(Voice& s, uint64_t seq) {
        s = Voice{};
        s.active = true;
        s.start_seq = seq;
    }
    Voice v_[MAX_VOICES];
};

} // namespace audio
