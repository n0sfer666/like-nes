#pragma once
#include <cstdint>
#include <cstring>
#include "input_types.hpp"

// Физическое состояние устройств, собранное из сырых событий детерминированно.
// Коалесценция идемпотентна по уровню (повторный опрос того же уровня → тот же результат)
// и суммирует дельты мыши → результат НЕ зависит от частоты опроса ОС (гейт #1).
// DeviceState хранит УРОВНИ (нажато/значение); edge (pressed/released) считает ActionMap
// по разнице с прошлым тиком.
namespace input {

constexpr int KBD_KEYS = 512;    // GLFW-диапазон
constexpr int MOUSE_BTNS = 8;
constexpr int PAD_BTNS = 32;
constexpr int PAD_AXES = 8;      // LX,LY,RX,RY,LT,RT,+2 запас

struct DeviceState {
    // Клавиатура (одна логическая): уровни клавиш.
    uint64_t keys[KBD_KEYS / 64] = {};
    // Мышь: уровни кнопок + аккумулированные дельты за тик (обнуляются при чтении).
    uint32_t mouse_btns = 0;
    int32_t acc_dx = 0, acc_dy = 0, acc_wheel = 0;   // накопление между тиками
    fix32 frame_dx, frame_dy, frame_wheel;           // залатченная дельта текущего тика
    // Геймпады по слотам: уровни кнопок + оси (fix32 raw).
    uint32_t pad_btns[MAX_DEVICES] = {};
    fix32 pad_axes[MAX_DEVICES][PAD_AXES];
    bool pad_connected[MAX_DEVICES] = {};
    bool focused = true;

    void reset_all_held() { // потеря фокуса / полный сброс — форс-release
        std::memset(keys, 0, sizeof(keys));
        mouse_btns = 0;
        for (int d = 0; d < MAX_DEVICES; ++d) {
            pad_btns[d] = 0;
            for (int a = 0; a < PAD_AXES; ++a) pad_axes[d][a] = fix32{};
        }
    }

    void apply(const RawEvent& e) {
        switch (e.kind) {
        case RawKind::KeyDown:   set_key(e.code, true); break;
        case RawKind::KeyUp:     set_key(e.code, false); break;
        case RawKind::MouseButtonDown: if (e.code < MOUSE_BTNS) mouse_btns |= (1u << e.code); break;
        case RawKind::MouseButtonUp:   if (e.code < MOUSE_BTNS) mouse_btns &= ~(1u << e.code); break;
        case RawKind::MouseMove: if (e.code == 0) acc_dx += e.value; else acc_dy += e.value; break; // code: 0=x, 1=y
        case RawKind::MouseWheel: acc_wheel += e.value; break;
        case RawKind::PadButtonDown: if (e.slot < MAX_DEVICES && e.code < PAD_BTNS) pad_btns[e.slot] |= (1u << e.code); break;
        case RawKind::PadButtonUp:   if (e.slot < MAX_DEVICES && e.code < PAD_BTNS) pad_btns[e.slot] &= ~(1u << e.code); break;
        case RawKind::PadAxis:       if (e.slot < MAX_DEVICES && e.code < PAD_AXES) pad_axes[e.slot][e.code] = fix32::from_raw(e.value); break;
        case RawKind::DeviceConnected:    if (e.slot < MAX_DEVICES) pad_connected[e.slot] = true; break;
        case RawKind::DeviceDisconnected: if (e.slot < MAX_DEVICES) { pad_connected[e.slot] = false; pad_btns[e.slot] = 0; for (int a=0;a<PAD_AXES;++a) pad_axes[e.slot][a]=fix32{}; } break;
        case RawKind::FocusLost: focused = false; reset_all_held(); break;
        case RawKind::TickMark: break; // граница тика — не меняет состояние
        }
    }

    bool key_down(int code) const { return code >= 0 && code < KBD_KEYS && ((keys[code >> 6] >> (code & 63)) & 1u); }
    bool mouse_down(int code) const { return code >= 0 && code < MOUSE_BTNS && ((mouse_btns >> code) & 1u); }
    bool pad_down(int slot, int code) const { return slot >= 0 && slot < MAX_DEVICES && code >= 0 && code < PAD_BTNS && ((pad_btns[slot] >> code) & 1u); }
    fix32 pad_axis(int slot, int code) const { return (slot >= 0 && slot < MAX_DEVICES && code >= 0 && code < PAD_AXES) ? pad_axes[slot][code] : fix32{}; }

    // Залатчить накопленные дельты мыши в поля кадра и обнулить аккумулятор (раз в тик,
    // ДО resolve). Сумма дельт не зависит от числа событий → независимость от частоты опроса.
    void latch_frame_delta() {
        frame_dx = fix32::from_int(acc_dx); frame_dy = fix32::from_int(acc_dy); frame_wheel = fix32::from_int(acc_wheel);
        acc_dx = acc_dy = acc_wheel = 0;
    }

private:
    void set_key(uint16_t code, bool v) {
        if (code >= KBD_KEYS) return;
        if (v) keys[code >> 6] |= (1ull << (code & 63));
        else   keys[code >> 6] &= ~(1ull << (code & 63));
    }
};

} // namespace input
