#include <cstdio>

#include "haptic.hpp"
#include "platform_haptic.hpp"
#include "platform_args.hpp"
#include "source.hpp"

namespace {
int fails, phone_starts, phone_pulses, phone_stops, phone_ends;
bool supported = true, pad_present = true;
float phone_strength;
int phone_ms;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("haptic: FAIL %s\n", why); } }
class Pad final : public input::GamepadSource {
public:
    int calls = 0, slot = -1, ms = -1;
    float low = -1, high = -1;
    bool init() override { return true; }
    void poll(input::InputEngine&) override {}
    const char* backend_name() const override { return "test"; }
    void set_rumble(int s, float l, float h, int t) override { ++calls; slot = s; low = l; high = h; ms = t; }
} pad;
}
input::GamepadSource* input::make_gamepad_source() { return pad_present ? &pad : nullptr; }
namespace haptic::phone {
bool init() { ++phone_starts; return supported; }
void pulse(float s, int ms) { ++phone_pulses; phone_strength = s; phone_ms = ms; }
void cancel() { ++phone_stops; }
void shutdown() { ++phone_ends; }
}

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    const haptic::Target phone{haptic::Kind::Phone}, p{haptic::Kind::Pad, 3};
    haptic::pulse(phone, 255, 20);
    check(phone_pulses == 0, "before init ignored");
    haptic::init(); haptic::init();
    check(phone_starts == 1, "startup once");
    haptic::pulse(phone, 255, 30);
    check(phone_pulses == 1 && phone_strength == 1 && phone_ms == 30, "phone full strength and dispatch");
    haptic::pulse(phone, 0, 30);
    check(phone_strength == 0 && phone_ms == 0, "zero strength cancels");
    haptic::pulse(phone, 255, 0);
    check(phone_strength == 0 && phone_ms == 0, "invalid duration cancels");
    haptic::pulse(p, 128, 60000);
    check(pad.calls == 1 && pad.slot == 3 && pad.low == 128.f / 255.f && pad.high == 128.f / 255.f && pad.ms == 60000, "actual pad dispatch");
    haptic::pulse({haptic::Kind::Pad, -1}, 255, 20);
    haptic::pulse({haptic::Kind::Pad, input::MAX_DEVICES}, 255, 20);
    haptic::pulse({static_cast<haptic::Kind>(99)}, 255, 20);
    check(pad.calls == 1, "invalid targets ignored");
    haptic::suspend(); haptic::suspend();
    check(pad.calls == 2 && pad.ms == 0 && phone_stops == 1, "suspend cancels owned output once");
    const int before = phone_pulses;
    haptic::pulse(phone, 255, 20); haptic::pulse(p, 255, 20);
    check(phone_pulses == before && pad.calls == 2, "suspended ignores game output");
    haptic::resume(); haptic::pulse(p, 0, 20);
    check(pad.calls == 3 && pad.low == 0 && pad.ms == 0, "pad zero strength cancels");
    pad_present = false;
    haptic::pulse(p, 255, 20); haptic::shutdown(); haptic::shutdown();
    check(pad.calls == 3 && phone_ends == 1, "missing backend and idempotent teardown");
    supported = false;
    haptic::init(); haptic::pulse(phone, 255, 20); haptic::shutdown();
    check(phone_pulses == before && phone_starts == 2 && phone_ends == 2, "no phone engine is a no-op");
    std::puts(fails ? "haptic: FAIL" : "haptic: PASS dispatch, bounds, cancellation, lifecycle, absent engines");
    return fails ? 1 : 0;
}
