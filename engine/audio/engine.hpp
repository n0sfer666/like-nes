#pragma once
#include <cstdint>

#include "audio_types.hpp"
#include "mixer.hpp"

// Типизированный sim-facing API (спека #3). Транслирует play/stop/set-* в AudioCommand и
// постит в mixer через lock-free SPSC. Команды таймштампятся в sample-time (детерминизм).
// Вызывается из sim-потока (продюсер SPSC); mixer — единственный консюмер (audio-callback).
namespace audio {

// Часы игры, ушедшие вперёд курсора миксера дальше RESYNC_LEAD, — след простоя вывода (перезапуск,
// reroute, suspend): курсор стоял, тики шли. Метка от таких часов ждала бы в голове очереди весь
// простой и держала бы за собой всё следующее (аудит #21 A·3·5).
constexpr uint64_t RESYNC_LEAD = SAMPLE_RATE / 4;
inline uint64_t resync_clock(uint64_t game_time, uint64_t cursor) {
    return game_time > cursor + RESYNC_LEAD ? cursor : game_time;
}

class AudioEngine {
public:
    explicit AudioEngine(Mixer& mixer) : mixer_(mixer) {}

    // Возвращает voice_id, либо 0 если SPSC-очередь полна (команда потеряна) — вызывающий
    // отличает потерю от валидного хэндла (0 = не сыграно).
    uint32_t play(uint64_t guid, const PlayParams& p, uint64_t sample_time) {
        AudioCommand c{};
        c.sample_time = sample_time;
        c.guid = guid;
        c.type = static_cast<uint32_t>(CmdType::Play);
        c.voice_id = next_id_++;
        c.gain = p.gain.raw;
        c.x = p.x.raw;
        c.y = p.y.raw;
        c.bus = static_cast<uint32_t>(p.bus);
        c.flags = (p.loop ? CMD_FLAG_LOOP : 0u) | (p.ducking ? CMD_FLAG_DUCK : 0u);
        c.priority = p.priority;
        return mixer_.post(c) ? c.voice_id : 0u;
    }

    void stop(uint32_t voice_id, uint64_t sample_time) {
        AudioCommand c{};
        c.type = static_cast<uint32_t>(CmdType::Stop);
        c.voice_id = voice_id;
        c.sample_time = sample_time;
        mixer_.post(c);
    }

    void set_bus_gain(Bus bus, fix32 gain, uint64_t sample_time) {
        AudioCommand c{};
        c.type = static_cast<uint32_t>(CmdType::SetBusGain);
        c.bus = static_cast<uint32_t>(bus);
        c.gain = gain.raw;
        c.sample_time = sample_time;
        mixer_.post(c);
    }

    // Громкость всего вывода поверх шин — ручка игрока (аудит #21 A·3·3), доля в [0,1].
    void set_master_gain(fix32 gain, uint64_t sample_time) {
        AudioCommand c{};
        c.type = static_cast<uint32_t>(CmdType::SetMasterGain);
        c.gain = gain.raw;
        c.sample_time = sample_time;
        mixer_.post(c);
    }

    void set_listener(fix32 x, fix32 y, uint64_t sample_time) {
        AudioCommand c{};
        c.type = static_cast<uint32_t>(CmdType::SetListener);
        c.x = x.raw;
        c.y = y.raw;
        c.sample_time = sample_time;
        mixer_.post(c);
    }

private:
    Mixer& mixer_;
    uint32_t next_id_ = 1;
};

} // namespace audio
