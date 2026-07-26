#pragma once
#include <cstdint>
#include "../src/fixed.hpp"

// Типы системы ввода: сырое async-событие ОС (input-поток) и детерм. per-tick InputFrame
// (то, что читает сим). Всё — POD, детерм. по значению. Оси — fix32 (целочисл. dead-zone).
namespace input {

constexpr int MAX_ACTIONS = 64; // держим в uint64 bitset
constexpr int MAX_AXES = 8;
constexpr int MAX_DEVICES = 8;  // физических устройств (пул)
constexpr int MAX_PLAYERS = 4;

enum class DeviceKind : uint8_t { None = 0, Keyboard, Mouse, Gamepad };

// Логический источник для биндинга (device-agnostic на уровне карты).
enum class SourceKind : uint8_t { None = 0, Key, MouseButton, MouseAxis, PadButton, PadAxis };

// Сырое событие ОС. seq — монотонный порядок продюсера (в пределах тика), НЕ wall-clock:
// в какой тик попадёт событие, решает дренаж sim @sync_point, не эта метка.
enum class RawKind : uint8_t {
    KeyDown, KeyUp,
    MouseButtonDown, MouseButtonUp, MouseMove, MouseWheel,
    PadButtonDown, PadButtonUp, PadAxis,
    DeviceConnected, DeviceDisconnected, FocusLost,
    TickMark, // граница тика в потоке событий (async-дренаж): sim коалесцирует до маркера
};

struct RawEvent {
    RawKind kind = RawKind::TickMark; // нейтральный дефолт (no-op в apply), не деструктивный
    DeviceKind device = DeviceKind::None;
    uint8_t slot = 0;    // индекс физического устройства (0..MAX_DEVICES)
    uint16_t code = 0;   // key/button/axis-код
    int32_t value = 0;   // ось: raw fix32; MouseMove: дельта(px, целое); иначе 0
    uint64_t seq = 0;    // монотонный порядок в input-потоке
};

// Детерминированный снапшот тика — единственный вход в сим. Хешируется в sim-hash.
struct InputFrame {
    uint32_t tick = 0;
    uint64_t held = 0;      // bitset Action: уровень (нажато сейчас)
    uint64_t pressed = 0;   // bitset Action: edge down в этот тик
    uint64_t released = 0;  // bitset Action: edge up в этот тик
    fix32 axes[MAX_AXES];   // разрешённые оси (пост dead-zone), fix32

    bool action_held(int a) const { return (held >> a) & 1u; }
    bool action_pressed(int a) const { return (pressed >> a) & 1u; }
    bool action_released(int a) const { return (released >> a) & 1u; }
};

} // namespace input
