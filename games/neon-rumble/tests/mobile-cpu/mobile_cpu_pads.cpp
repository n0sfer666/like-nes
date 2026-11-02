#include "mobile_cpu_pads.hpp"

#include "codes.hpp"
#include "source.hpp"

namespace {
class Pads final : public input::GamepadSource {
public:
    bool enabled = false, disconnect = false;
    bool init() override { ticks = 0; return enabled; }
    const char* backend_name() const override { return "CPU fixture pad"; }
    void set_rumble(int, float, float, int) override {}
    void poll(input::InputEngine& engine) override {
        ++ticks;
        const auto emit = [&](input::RawKind kind, int code = 0, int value = 0) {
            engine.post({kind, input::DeviceKind::Gamepad, 3, static_cast<uint16_t>(code), value, ticks});
        };
        if (ticks == 1) {
            emit(input::RawKind::DeviceConnected);
            emit(input::RawKind::PadButtonDown, input::code::PadX);
        }
        if (ticks == 2) emit(input::RawKind::PadButtonUp, input::code::PadX);
        if (ticks == 3) emit(input::RawKind::PadAxis, input::code::LX, fix32::from_int(1).raw);
        if (disconnect && ticks == 61) emit(input::RawKind::DeviceDisconnected);
    }
private:
    uint64_t ticks = 0;
};
Pads pads;
}

void mobile_cpu_pad_mode(bool enabled, bool disconnect) { pads.enabled = enabled; pads.disconnect = disconnect; }
input::GamepadSource* input::make_gamepad_source() { return pads.enabled ? &pads : nullptr; }
