#pragma once
#include <cstdint>

#include "audio_types.hpp"
#include "fix_math.hpp"
#include "spsc.hpp"
#include "voice.hpp"

// Микшер (спека #3). Живёт в audio-callback (RT-safe: без локов/heap/I/O). Тянет команды
// из lock-free SPSC (продюсер — sim/engine), микширует голоса через пан/аттенюацию → шины →
// ducking → master. Seam Float/Fix32: control-математика всегда fix32 (детерм.), различается
// только MAC-петля семплов — Fix32 даёт байт-golden (гейт #1), Float — прод best-effort.
namespace audio {

constexpr uint32_t CMD_CAP = 1024;   // степень двойки
constexpr uint32_t MAX_SOURCES = 16; // реестр guid→PCM (регистрируется вне RT)

enum class Backend { Float, Fix32 };

struct Source {
    uint64_t guid = 0;
    const int16_t* pcm = nullptr;
    uint32_t frames = 0;
    SampleRing* ring = nullptr;
};

class Mixer {
public:
    explicit Mixer(Backend b);

    // Продюсер (sim/engine-поток): поставить команду. false — очередь полна.
    bool post(const AudioCommand& c) { return commands_.push(c); }

    // Реестр источников (вне RT, до play): guid → резидентный PCM или стрим-ring.
    void register_source(uint64_t guid, const int16_t* pcm, uint32_t frames, SampleRing* ring);

    // Callback устройства (RT-safe). Микширует frames стерео-фреймов в out (interleaved int16).
    void mix(uint32_t frames, int16_t* out);

    uint64_t cursor() const { return cursor_; }
    uint64_t underruns() const { return underruns_; }
    uint32_t peak_voices() const { return peak_voices_; }

private:
    void drain_commands(uint32_t frames);
    void apply(const AudioCommand& c, uint32_t offset);
    void recompute_gains(uint32_t frames);
    const Source* find_source(uint64_t guid) const;
    void mix_fix(uint32_t frames, int16_t* out);
    void mix_float(uint32_t frames, int16_t* out);

    Backend backend_;
    SpscQueue<AudioCommand, CMD_CAP> commands_;
    VoicePool pool_;
    Source sources_[MAX_SOURCES];
    uint32_t source_count_ = 0;

    fix32 bus_gain_[static_cast<uint32_t>(Bus::Count)];
    fix32 master_;
    fix32 duck_env_;    // duck-огибающая (рампится ПО СЕМПЛУ в mix-петле → block-независимо)
    fix32 listener_x_, listener_y_;
    uint64_t cursor_ = 0;
    uint64_t seq_ = 0;
    uint64_t underruns_ = 0;
    uint32_t peak_voices_ = 0;
};

} // namespace audio
