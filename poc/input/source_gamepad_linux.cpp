#include "source.hpp"
#include "codes.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <set>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

// Linux native gamepad: evdev (/dev/input/event*), FF_RUMBLE. Hot-plug = рескан каталога
// (libudev — уточнение). Скелет: собирается в CI (ubuntu); live-валидация — follow-up (нет HW).
namespace input {
namespace c = input::code;

namespace {
int map_key(int code) {
    switch (code) {
    case BTN_SOUTH: return c::PadA; case BTN_EAST: return c::PadB;
    case BTN_NORTH: return c::PadY; case BTN_WEST: return c::PadX;
    case BTN_TL: return c::LB; case BTN_TR: return c::RB;
    case BTN_SELECT: return c::Back; case BTN_START: return c::Start;
    case BTN_THUMBL: return c::LStick; case BTN_THUMBR: return c::RStick;
    default: return -1;
    }
}
int map_abs(int code, int& is_axis) { // возвращает наш axis-код или dpad-hat
    is_axis = 1;
    switch (code) {
    case ABS_X: return c::LX; case ABS_Y: return c::LY;
    case ABS_RX: return c::RX; case ABS_RY: return c::RY;
    case ABS_Z: return c::LT; case ABS_RZ: return c::RT;
    default: is_axis = 0; return code; // ABS_HAT0X/Y обрабатываем отдельно
    }
}

struct Dev { int fd = -1; int slot = -1; input_absinfo abs[ABS_CNT]; };

class LinuxGamepadSource : public GamepadSource {
public:
    bool init() override { return true; }

    void poll(InputEngine& e) override {
        rescan(e);
        for (Dev& d : devs_) {
            if (d.fd < 0) continue;
            input_event ev;
            ssize_t n;
            while ((n = read(d.fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
                if (ev.type == EV_KEY) {
                    int code = map_key(ev.code);
                    if (code >= 0) e.post({ev.value ? RawKind::PadButtonDown : RawKind::PadButtonUp,
                                           DeviceKind::Gamepad, (uint8_t)d.slot, (uint16_t)code, 0, seq_++});
                } else if (ev.type == EV_ABS) {
                    handle_abs(e, d, ev);
                }
            }
            if (n < 0 && (errno == ENODEV)) { close_dev(e, d); }
        }
        devs_.erase(std::remove_if(devs_.begin(), devs_.end(), [](const Dev& d) { return d.fd < 0; }), devs_.end());
    }

    void set_rumble(int slot, float low, float high, int ms) override {
        Dev* d = by_slot(slot);
        if (!d || d->fd < 0) return;
        ff_effect fx{};
        fx.type = FF_RUMBLE; fx.id = -1;
        fx.u.rumble.strong_magnitude = (uint16_t)(low * 65535.0f);
        fx.u.rumble.weak_magnitude = (uint16_t)(high * 65535.0f);
        fx.replay.length = (uint16_t)ms;
        if (ioctl(d->fd, EVIOCSFF, &fx) < 0) return;
        input_event play{}; play.type = EV_FF; play.code = (uint16_t)fx.id; play.value = 1;
        ssize_t w = write(d->fd, &play, sizeof(play)); (void)w;
    }

    const char* backend_name() const override { return "evdev (Linux)"; }

private:
    void handle_abs(InputEngine& e, Dev& d, const input_event& ev) {
        if (ev.code == ABS_HAT0X) { emit_dpad(e, d.slot, c::DpLeft, ev.value < 0); emit_dpad(e, d.slot, c::DpRight, ev.value > 0); return; }
        if (ev.code == ABS_HAT0Y) { emit_dpad(e, d.slot, c::DpUp, ev.value < 0); emit_dpad(e, d.slot, c::DpDown, ev.value > 0); return; }
        int is_axis; int code = map_abs(ev.code, is_axis);
        if (!is_axis || ev.code >= ABS_CNT) return;
        const input_absinfo& ai = d.abs[ev.code];
        int range = ai.maximum - ai.minimum;
        float norm = range > 0 ? (2.0f * (ev.value - ai.minimum) / range - 1.0f) : 0.0f;
        if (code == c::LT || code == c::RT) norm = range > 0 ? (float)(ev.value - ai.minimum) / range : 0.0f;
        e.post({RawKind::PadAxis, DeviceKind::Gamepad, (uint8_t)d.slot, (uint16_t)code, fix32::from_float(norm).raw, seq_++});
    }
    void emit_dpad(InputEngine& e, int slot, int code, bool pressed) {
        e.post({pressed ? RawKind::PadButtonDown : RawKind::PadButtonUp, DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, 0, seq_++});
    }
    Dev* by_slot(int slot) { for (Dev& d : devs_) if (d.slot == slot) return &d; return nullptr; }

    bool is_gamepad(int fd) {
        unsigned long bits[(KEY_MAX + 8) / (8 * sizeof(long))] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;
        return (bits[BTN_GAMEPAD / (8 * sizeof(long))] >> (BTN_GAMEPAD % (8 * sizeof(long)))) & 1ul;
    }

    void rescan(InputEngine& e) {
        DIR* dir = opendir("/dev/input");
        if (!dir) return;
        dirent* de;
        while ((de = readdir(dir))) {
            if (strncmp(de->d_name, "event", 5) != 0) continue;
            char path[64]; snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
            if (opened_.count(path)) continue;
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            if (!is_gamepad(fd)) { close(fd); continue; }
            int slot = free_slot();
            if (slot < 0) { close(fd); continue; }
            Dev d; d.fd = fd; d.slot = slot;
            for (int a = 0; a < ABS_CNT; ++a) ioctl(fd, EVIOCGABS(a), &d.abs[a]);
            devs_.push_back(d); opened_.insert(path); paths_[slot] = path;
            e.post({RawKind::DeviceConnected, DeviceKind::Gamepad, (uint8_t)slot, 0, 0, seq_++});
        }
        closedir(dir);
    }
    int free_slot() { bool used[MAX_DEVICES] = {}; for (Dev& d : devs_) if (d.slot >= 0) used[d.slot] = true; for (int s = 0; s < MAX_DEVICES; ++s) if (!used[s]) return s; return -1; }
    void close_dev(InputEngine& e, Dev& d) {
        e.post({RawKind::DeviceDisconnected, DeviceKind::Gamepad, (uint8_t)d.slot, 0, 0, seq_++});
        close(d.fd); opened_.erase(paths_[d.slot]); d.fd = -1; d.slot = -1;
    }

    std::vector<Dev> devs_;
    std::set<std::string> opened_;
    std::string paths_[MAX_DEVICES];
    uint64_t seq_ = 1'000'000;
};
} // namespace

GamepadSource* make_gamepad_source() { static LinuxGamepadSource s; return &s; }

} // namespace input
