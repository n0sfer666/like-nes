#pragma once
#include <cstdint>

#include "../src/fixed.hpp"

// Общие типы аудио-подсистемы (спека #3). Внутренний микс — фикс. rate/каналы;
// команды таймштампятся в sample-time (tick N → sample N*SAMPLES_PER_TICK), НЕ wall-clock,
// поэтому тайминг устройства не влияет ни на sim, ни на реплей (инвариант #2/#3 спеки #1).
namespace audio {

constexpr uint32_t SAMPLE_RATE = 48000;
constexpr uint32_t OUT_CHANNELS = 2;   // стерео-выход
constexpr uint32_t TICK_HZ = 60;
constexpr uint32_t SAMPLES_PER_TICK = SAMPLE_RATE / TICK_HZ; // 800 фреймов/тик
constexpr uint32_t MAX_VOICES = 32;    // фикс. пул — без per-callback heap

enum class Bus : uint32_t { Music = 0, Sfx = 1, Ambience = 2, Ui = 3, Count = 4 };

constexpr uint32_t CMD_FLAG_LOOP = 1u;
constexpr uint32_t CMD_FLAG_DUCK = 2u; // голос пригашает music-шину (sidechain-lite)

enum class CmdType : uint32_t { Play = 0, Stop = 1, SetBusGain = 2, SetListener = 3 };

// POD-команда в lock-free SPSC (sim→mixer). Все поля по значению → детерминизм.
struct AudioCommand {
    uint64_t sample_time; // абсолютный фрейм, когда команда вступает в силу
    uint64_t guid;        // Play: ассет-источник
    uint32_t type;        // CmdType
    uint32_t voice_id;    // Play: назначенный продюсером хэндл; Stop: какой голос
    int32_t gain;         // fix32.raw (Play: громкость; SetBusGain: gain)
    int32_t x;            // fix32.raw (Play: pos.x; SetListener: x)
    int32_t y;            // fix32.raw (Play: pos.y; SetListener: y)
    uint32_t bus;         // Bus
    uint32_t flags;       // CMD_FLAG_*
    uint32_t priority;    // для voice-stealing
};

// Параметры play() со стороны игры (типизированный API).
struct PlayParams {
    Bus bus = Bus::Sfx;
    fix32 gain = fix32::from_int(1);
    fix32 x = fix32();
    fix32 y = fix32();
    bool loop = false;
    bool ducking = false;
    uint8_t priority = 128;
};

} // namespace audio
