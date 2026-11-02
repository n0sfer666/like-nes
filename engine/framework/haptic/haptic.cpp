#include "haptic.hpp"

#include <array>
#include <chrono>
#include <cstdio>

#include "platform_haptic.hpp"
#include "source.hpp"

namespace haptic {
namespace {
using Clock = std::chrono::steady_clock;
thread_local bool ready = false, active = false, phone_available = false;
thread_local std::array<Clock::time_point, input::MAX_DEVICES> until;
}

void init() {
    if (ready) return;
    ready = active = true;
    phone_available = phone::init();
    std::fprintf(stderr, "[haptic] phone %s\n", phone_available ? "ready" : "unavailable (no-op)");
}

void pulse(Target target, uint8_t strength, uint16_t ms) {
    if (!ready || !active) return;
    if (strength == 0 || ms == 0) { strength = 0; ms = 0; }
    if (target.kind == Kind::Phone) { if (phone_available) phone::pulse(static_cast<float>(strength) / 255.f, ms); return; }
    if (target.kind != Kind::Pad || target.pad_slot < 0 || target.pad_slot >= input::MAX_DEVICES) return;
    if (auto* pads = input::make_gamepad_source()) {
        const float level = static_cast<float>(strength) / 255.f;
        pads->set_rumble(target.pad_slot, level, level, ms);
        until[target.pad_slot] = ms > 0 ? Clock::now() + std::chrono::milliseconds(ms) : Clock::time_point{};
    }
}

void suspend() {
    if (!ready || !active) return;
    active = false;
    if (phone_available) phone::cancel();
    auto* pads = input::make_gamepad_source();
    for (int slot = 0; slot < input::MAX_DEVICES; ++slot) {
        if (pads && until[slot] > Clock::now()) pads->set_rumble(slot, 0, 0, 0);
        until[slot] = {};
    }
}
void resume() { if (ready) active = true; }
void shutdown() {
    if (!ready) return;
    suspend();
    phone::shutdown();
    ready = false;
}
} // namespace haptic
