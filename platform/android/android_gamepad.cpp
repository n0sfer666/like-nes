#include "android_gamepad.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "android_pad_device.hpp"
#include "android_pad_events.hpp"
#include "codes.hpp"
#include "source.hpp"

namespace {
namespace c = input::code;
using Clock = std::chrono::steady_clock;

struct Slot {
    int id = -1, reported = -1;
    AndroidPadDevice device;
    uint32_t keys = 0, hat = 0, buttons = 0, triggers = 0;
    std::array<int32_t, input::PAD_AXES> axes{}, previous_axes{};
    bool haptic_warned = false;
    Clock::time_point rumble_until{};
};

class AndroidPads final : public input::GamepadSource {
public:
    JavaVM* vm = nullptr;
    bool init() override { return vm != nullptr; }
    const char* backend_name() const override { return "Android NDK"; }
    input::PadInfo pad_info(int index) const override {
        return index >= 0 && index < input::MAX_DEVICES && slots[index].id >= 0 ? slots[index].device.info : input::PadInfo{};
    }
    void set_rumble(int index, float low, float high, int ms) override {
        if (!vm || index < 0 || index >= input::MAX_DEVICES || slots[index].id < 0) return;
        Slot& s = slots[index];
        const float strength = std::isfinite(low) && std::isfinite(high) ? std::max(low, high) : 0.f;
        const bool played = android_pad_rumble(vm, s.id, strength, ms);
        if (played) s.rumble_until = strength > 0 && ms > 0 ? Clock::now() + std::chrono::milliseconds(ms) : Clock::time_point{};
        if (!played && !s.haptic_warned) {
            std::fprintf(stderr, "[input] pad slot %d haptics unavailable\n", index);
            s.haptic_warned = true;
        }
    }

    void refresh() { scan_at = {}; }
    void cancel() {
        for (Slot& s : slots) {
            // Останавливаем только ещё действующий эффект этого клиента, не чужой мотор.
            if (vm && s.id >= 0 && s.rumble_until > Clock::now()) (void)android_pad_rumble(vm, s.id, 0, 0);
            s.rumble_until = {};
            s.keys = s.hat = s.triggers = 0; s.axes.fill(0);
        }
    }

    void poll(input::InputEngine& engine) override {
        if (!vm) return;
        if (Clock::now() >= scan_at) scan();
        for (int index = 0; index < input::MAX_DEVICES; ++index) {
            Slot& s = slots[index];
            if (s.id != s.reported) {
                if (s.reported >= 0) emit(engine, index, input::RawKind::DeviceDisconnected);
                s.buttons = 0; s.previous_axes.fill(0);
                if (s.id >= 0) emit(engine, index, input::RawKind::DeviceConnected);
                s.reported = s.id;
            }
            if (s.id < 0) continue;
            const uint32_t next = s.keys | s.hat;
            for (int button = 0; button < input::PAD_BTNS; ++button) {
                const uint32_t bit = 1u << button;
                if (((next ^ s.buttons) & bit) != 0)
                    emit(engine, index, (next & bit) ? input::RawKind::PadButtonDown : input::RawKind::PadButtonUp, button);
            }
            auto axes = s.axes;
            if (s.triggers & 1u) axes[c::LT] = fix32::from_int(1).raw;
            if (s.triggers & 2u) axes[c::RT] = fix32::from_int(1).raw;
            for (int axis = 0; axis < input::PAD_AXES; ++axis)
                if (axes[axis] != s.previous_axes[axis]) emit(engine, index, input::RawKind::PadAxis, axis, axes[axis]);
            s.buttons = next; s.previous_axes = axes;
        }
    }

    int event(const AInputEvent* event) {
        if (!vm || !event || !android_pad_source(AInputEvent_getSource(event))) return 0;
        const int type = AInputEvent_getType(event);
        const int key = type == AINPUT_EVENT_TYPE_KEY ? AKeyEvent_getKeyCode(event) : -1;
        const int button = android_pad_button(key);
        const uint32_t trigger = key == AKEYCODE_BUTTON_L2 ? 1u : key == AKEYCODE_BUTTON_R2 ? 2u : 0;
        if (type != AINPUT_EVENT_TYPE_MOTION && button < 0 && trigger == 0) return 0;
        const int index = connect(AInputEvent_getDeviceId(event));
        if (index < 0) return 0;
        Slot& s = slots[index];
        if (type == AINPUT_EVENT_TYPE_KEY) {
            const int action = AKeyEvent_getAction(event);
            if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) return 0;
            const bool down = action == AKEY_EVENT_ACTION_DOWN && !(AKeyEvent_getFlags(event) & AKEY_EVENT_FLAG_CANCELED);
            if (trigger) { s.triggers = down ? s.triggers | trigger : s.triggers & ~trigger; return 1; }
            const uint32_t bit = 1u << button;
            s.keys = down ? s.keys | bit : s.keys & ~bit;
        } else {
            const int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
            if (action == AMOTION_EVENT_ACTION_CANCEL) { s.keys = s.hat = s.triggers = 0; s.axes.fill(0); return 1; }
            if (action != AMOTION_EVENT_ACTION_MOVE || AMotionEvent_getPointerCount(event) == 0) return 0;
            const auto value = [&](int axis) { return AMotionEvent_getAxisValue(event, axis, 0); };
            s.axes[c::LX] = android_pad_axis(value(AMOTION_EVENT_AXIS_X)).raw;
            s.axes[c::LY] = android_pad_axis(value(AMOTION_EVENT_AXIS_Y)).raw;
            s.axes[c::RX] = android_pad_axis(value(s.device.rx)).raw;
            s.axes[c::RY] = android_pad_axis(value(s.device.ry)).raw;
            s.axes[c::LT] = android_pad_axis(std::max(value(AMOTION_EVENT_AXIS_LTRIGGER), value(AMOTION_EVENT_AXIS_BRAKE)), true).raw;
            s.axes[c::RT] = android_pad_axis(std::max(value(AMOTION_EVENT_AXIS_RTRIGGER), value(AMOTION_EVENT_AXIS_GAS)), true).raw;
            s.hat = 0;
            const fix32 x = android_pad_axis(value(AMOTION_EVENT_AXIS_HAT_X));
            const fix32 y = android_pad_axis(value(AMOTION_EVENT_AXIS_HAT_Y));
            if (x.raw < 0) s.hat |= 1u << c::DpLeft;
            if (x.raw > 0) s.hat |= 1u << c::DpRight;
            if (y.raw < 0) s.hat |= 1u << c::DpUp;
            if (y.raw > 0) s.hat |= 1u << c::DpDown;
        }
        return 1;
    }

private:
    int connect(int id) {
        if (id < 0) return -1;
        for (int i = 0; i < input::MAX_DEVICES; ++i) if (slots[i].id == id) return i;
        AndroidPadDevice device;
        if (!android_pad_device(vm, id, device) || !device.gamepad) return -1;
        for (int i = 0; i < input::MAX_DEVICES; ++i) {
            // Пока отключение не опубликовано отдельным poll, слот не отдаём новому id:
            // disconnect+connect в одном drain скрыли бы потерю от Lobby.
            if (slots[i].id >= 0 || slots[i].reported >= 0) continue;
            const int reported = slots[i].reported;
            slots[i] = Slot{};
            slots[i].reported = reported; slots[i].id = id; slots[i].device = device;
            return i;
        }
        return -1;
    }
    void scan() {
        scan_at = Clock::now() + std::chrono::milliseconds(100);
        std::array<jint, 128> ids{};
        size_t count = 0;
        if (!android_pad_ids(vm, ids, count)) {
            if (!warned) std::fprintf(stderr, "[input] Android device inventory JNI failed\n");
            warned = true;
            // Не оставляем held при недоступном инвентаре. Повторный успешный scan подключит
            // устройство заново; лобби обработает потерю по обычному протоколу.
            cancel();
            for (Slot& s : slots) s.id = -1;
            return;
        }
        for (Slot& s : slots)
            if (s.id >= 0 && std::find(ids.begin(), ids.begin() + count, s.id) == ids.begin() + count) {
                s.id = -1; s.keys = s.hat = s.triggers = 0; s.axes.fill(0);
            }
        for (size_t i = 0; i < count; ++i) (void)connect(ids[i]);
    }
    void emit(input::InputEngine& engine, int index, input::RawKind kind, int code = 0, int value = 0) {
        engine.post({kind, input::DeviceKind::Gamepad, static_cast<uint8_t>(index),
                     static_cast<uint16_t>(code), value, sequence++});
    }
    std::array<Slot, input::MAX_DEVICES> slots;
    Clock::time_point scan_at{};
    uint64_t sequence = 1'000'000;
    bool warned = false;
};

thread_local AndroidPads pads;
} // namespace

void android_gamepad_begin(JavaVM* vm) { pads.cancel(); pads = AndroidPads{}; pads.vm = vm; }
void android_gamepad_end() { pads.cancel(); pads = AndroidPads{}; }
void android_gamepad_refresh() { pads.refresh(); }
void android_gamepad_cancel() { pads.cancel(); }
int32_t android_gamepad_event(const AInputEvent* event) { return pads.event(event); }
input::GamepadSource* input::make_gamepad_source() { return &pads; }
