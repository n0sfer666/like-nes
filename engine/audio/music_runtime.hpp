#pragma once

#include <memory>

#include <array>
#include <cstdint>

#include "music_envelope.hpp"
#include "music_stats.hpp"
#include "music_transport.hpp"

namespace audio {
class Mixer;
class MusicStream;
struct MusicWish { uint32_t track = 0, fade_samples = 0; };
// Сумма двух PCM*Q16 без int16 clamp; underruns считаются по активным slot-frames, не по priming.
struct MusicMix { int64_t left = 0, right = 0; uint32_t underruns = 0; };

// Private controller: игра передаёт индексы, callback единолично меняет состояния слотов.
// Runtime живёт до stop/join worker и callback; assets остаются у внешнего владельца worker.
class MusicRuntime {
public:
    explicit MusicRuntime(uint32_t track_count);
    static bool bind(Mixer& mixer, const std::shared_ptr<MusicRuntime>& runtime);
    // Один game producer. Отказ при full/closed/невалидном индексе; caller сохраняет pending wish.
    bool post(MusicWish wish);
    MusicMix next(); // один RT consumer, один выходной sample; без heap/locks/IO
    MusicTransport& transport() { return transport_; } // подключение worker вне callback
    MusicStats stats() const;

private:
    friend class Mixer;
    friend class MusicStream;
    bool claim_binding();
    void record_underruns(uint64_t count);
    enum class Phase : uint8_t { Empty, Priming, Playing };
    enum class Gain : uint8_t { Held, Rising, Falling };
    struct Slot {
        Phase phase = Phase::Empty;
        uint32_t track = 0, fade_samples = 0;
        uint64_t generation = 0;
        MusicEnvelope gain;
        StereoFrame primer{};
        bool has_primer = false;
        Gain direction = Gain::Held;
    };
    struct PendingRequest { MusicLoad load{}; bool active = false; };

    void update();
    void handle(MusicWish wish);
    bool new_generation(uint64_t& generation);
    void begin_load(uint32_t slot, MusicWish wish);
    void retire(uint32_t slot);
    void retry_requests();
    void start_deferred(uint32_t slot);
    static void hold(Slot& slot);
    static void ramp(Slot& slot, int32_t target, uint32_t frames);

    uint32_t track_count_ = 0;
    bool valid_ = false;
    MusicTransport transport_;
    SpscQueue<MusicWish, MUSIC_REQUEST_CAP> wishes_;
    std::array<Slot, MUSIC_SLOTS> slots_{};
    std::array<PendingRequest, MUSIC_SLOTS> pending_requests_{};
    std::atomic<bool> bound_once_{false};
    std::atomic<uint64_t> mix_underruns_{0};
    uint64_t generation_ = 0;
    int32_t release_slot_ = -1;
    bool deferred_ = false;
    uint32_t deferred_track_ = 0, deferred_fade_ = 0;
};
} // namespace audio
