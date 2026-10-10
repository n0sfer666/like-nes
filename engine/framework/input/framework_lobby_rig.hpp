#pragma once
#include <cstdio>

#include "codes.hpp"
#include "input_engine.hpp"
#include "lobby.hpp"

namespace framework::input::test {

inline int fails = 0;
inline int cases = 0;

inline void check(bool ok, const char* what) {
    ++cases;
    if (ok) return;
    std::printf("  FAIL: %s\n", what);
    ++fails;
}

inline int verdict(const char* name) {
    std::printf("%s: %s - %d cases\n", name, fails == 0 ? "PASS" : "FAIL", cases);
    return fails == 0 ? 0 : 1;
}

enum : uint16_t { KEY_J = 74, KEY_RIGHT = 262, KEY_LEFT = 263, KEY_KP1 = 321 };
enum Action : int { JUMP = 0, PUNCH = 1 };

inline ::input::Source key(uint16_t code) { return {::input::SourceKind::Key, code, 1}; }
inline ::input::Source pad(uint16_t code) { return {::input::SourceKind::PadButton, code, 1}; }

inline ::input::ActionLayout p1_layout() {
    ::input::ActionLayout l;
    l.bind(JUMP, key(::input::code::Space));
    l.bind(PUNCH, key(KEY_J));
    l.bind(JUMP, pad(::input::code::PadA));
    l.bind(PUNCH, pad(::input::code::PadX));
    l.bind_axis(0, key(::input::code::D), key(::input::code::A), fix32{});
    return l;
}

// Половина игрока 2 у владельца: стрелки — ось, нампад — кнопки.
inline ::input::ActionLayout p2_layout(uint16_t punch_key = KEY_KP1) {
    ::input::ActionLayout l;
    l.bind(PUNCH, key(punch_key));
    l.bind(JUMP, pad(::input::code::PadA));
    l.bind(PUNCH, pad(::input::code::PadX));
    l.bind_axis(0, key(KEY_RIGHT), key(KEY_LEFT), fix32{});
    return l;
}

struct Rig {
    ::input::ActionMap map;
    ::input::InputEngine engine{map};
    Lobby lobby{map, 2};
    uint32_t tick = 0;
    uint64_t seq = 0;

    // Первый update лобби только засевает прошлое состояние, поэтому сценарии начинают со второго.
    explicit Rig(uint16_t p2_punch = KEY_KP1, bool seed = true) {
        check(map.set_layout(0, p1_layout()) && map.set_layout(1, p2_layout(p2_punch)), "both layouts are set");
        if (seed) step();
    }

    void post(::input::RawKind k, ::input::DeviceKind d, uint8_t slot, uint16_t code) {
        engine.post(::input::RawEvent{k, d, slot, code, 0, seq++});
    }
    void plug(uint8_t slot) { post(::input::RawKind::DeviceConnected, ::input::DeviceKind::Gamepad, slot, 0); }
    void unplug(uint8_t slot) { post(::input::RawKind::DeviceDisconnected, ::input::DeviceKind::Gamepad, slot, 0); }
    void button(uint8_t slot, uint16_t b, bool down) {
        post(down ? ::input::RawKind::PadButtonDown : ::input::RawKind::PadButtonUp, ::input::DeviceKind::Gamepad,
             slot, b);
    }
    void keypress(uint16_t code, bool down) {
        post(down ? ::input::RawKind::KeyDown : ::input::RawKind::KeyUp, ::input::DeviceKind::Keyboard, 0, code);
    }

    void step() {
        engine.drain();
        lobby.update(engine.device());
        for (int p = 0; p < 2; ++p) engine.resolve(tick, p);
        ++tick;
    }
};

} // namespace framework::input::test
