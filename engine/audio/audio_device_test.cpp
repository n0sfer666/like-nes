#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "audio_types.hpp"
#include "device.hpp"
#include "engine.hpp"
#include "miniaudio.h"
#include "mixer.hpp"

// Аудит #21 A·3·5: бэкенд остановил устройство (вынули наушники) — игра поднимает его сама, а не
// играет молча до перезапуска; бэкенд, поднявший поток сам (WASAPI при смене вывода), не
// перебивается; вторая потеря сразу после перезапуска — конец попыток; часы игры, ушедшие за
// простой вперёд курсора, возвращаются к нему. Null-бэкенд miniaudio даёт тот же ma_device и тот
// же поток уведомлений; остановку бэкенда даёт ma_device_stop мимо обёртки, а уведомления
// CoreAudio и Pulse, которые приходят без смены состояния, — прямой вызов коллбэка.
namespace audio {
struct DeviceTestAccess {
    static ma_device* dev(MiniaudioDevice& d) { return static_cast<ma_device*>(d.device_); }
    static void backend_stops(MiniaudioDevice& d) { ma_device_stop(dev(d)); }
    static void backend_starts(MiniaudioDevice& d) { ma_device_start(dev(d)); }
    static void notify(MiniaudioDevice& d, ma_device_notification_type type) {
        ma_device_notification n{};
        n.pDevice = dev(d);
        n.type = type;
        if (dev(d)->onNotification) dev(d)->onNotification(&n);
    }
    static void self_routing(MiniaudioDevice& d, bool on) { d.self_routing_ = on; }
    static void force_state(MiniaudioDevice& d, ma_device_state st) {
        std::atomic_ref<ma_device_state>(dev(d)->state.value).store(st);
    }
};
} // namespace audio

using namespace audio;
using A = DeviceTestAccess;

namespace {

constexpr uint64_t GUID = 1;
constexpr uint32_t SRC_FRAMES = 480;
std::atomic<uint64_t> g_frames{0}, g_loud{0};
int fails = 0;

void render(void* user, uint32_t frames, int16_t* out) {
    static_cast<Mixer*>(user)->mix(frames, out);
    for (uint32_t i = 0; i < frames * OUT_CHANNELS; ++i) {
        if (out[i]) {
            ++g_loud;
            break;
        }
    }
    g_frames += frames;
}

// По часам, а не числом итераций: sleep на Windows округляется до ~15.6 мс.
bool grows(const std::atomic<uint64_t>& c, int ms) {
    const uint64_t from = c.load();
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < until) {
        if (c.load() > from) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return c.load() > from;
}

bool renders() { return grows(g_frames, 2000); }

// Поток игры читает курсор, пока устройство пишет его, и без команды следом: только так
// неатомарный курсор виден TSan — push в очередь сам упорядочил бы чтение с записью.
bool cursor_moves(const Mixer& mix, uint64_t from) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < until) {
        if (mix.cursor() >= from + SAMPLE_RATE / 10) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

void wait_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void past_window() { wait_ms(600); }
void wait_out(MiniaudioDevice& dev) { dev.poll(); past_window(); }
void reopen(MiniaudioDevice& dev, Mixer& mix) { dev.stop(); dev.start(render, &mix, true); }

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("[audio-device] FAIL: %s\n", what);
        ++fails;
    }
}

} // namespace

int main() {
    int16_t src[SRC_FRAMES];
    for (int16_t& s : src) s = 8000;
    Mixer mix(Backend::Fix32);
    mix.register_source(GUID, src, SRC_FRAMES, nullptr);
    AudioEngine eng(mix);
    MiniaudioDevice dev;
    if (!dev.start(render, &mix, /*null_backend=*/true) || !renders()) {
        std::printf("audio-device: FAIL - the null backend does not render, nothing below is tested\n");
        return 1;
    }
    check(dev.poll() == DeviceEvent::None, "a running device reports nothing");

    A::backend_stops(dev);
    const uint64_t stopped_at = mix.cursor();
    const uint64_t game_clock = stopped_at + SAMPLE_RATE * 2;
    check(dev.poll() == DeviceEvent::None, "a stop is not acted on inside the 500 ms window");
    past_window();
    check(dev.poll() == DeviceEvent::Restarted, "a device the backend stopped is restarted");
    check(cursor_moves(mix, stopped_at), "the restarted device moves the mixer cursor again");
    eng.play(GUID, PlayParams{}, resync_clock(game_clock, mix.cursor()));
    check(grows(g_loud, 100), "a sound stamped with the resynced game clock plays at once");
    wait_ms(50);
    eng.play(GUID, PlayParams{}, game_clock);
    check(!grows(g_loud, 100), "a sound stamped with a game clock that ran through the outage is late");
    check(dev.poll() == DeviceEvent::None, "one loss is one restart");

    A::backend_stops(dev);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::Failed, "a second loss within 5 s of a restart is final");
    check(dev.poll() == DeviceEvent::None && !A::dev(dev), "a final loss leaves the device down");

    dev.stop();
    check(dev.start(render, &mix, true) && renders(), "the device starts again after stop");
    A::notify(dev, ma_device_notification_type_stopped);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::Restarted,
          "a stop notified on a device still marked started is restarted, with a fresh budget");

    reopen(dev, mix);
    A::backend_stops(dev);
    dev.poll();
    wait_ms(300);
    A::notify(dev, ma_device_notification_type_started);
    A::notify(dev, ma_device_notification_type_stopped);
    check(dev.poll() == DeviceEvent::None, "a fresh notification restarts the 500 ms window");
    wait_ms(300);
    check(dev.poll() == DeviceEvent::None, "a series of notifications does not add up to one window");
    wait_ms(300);
    check(dev.poll() == DeviceEvent::Restarted, "the window ends 500 ms after the last notification");

    reopen(dev, mix);
    A::backend_stops(dev);
    A::notify(dev, ma_device_notification_type_rerouted);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::Restarted, "a device left stopped after a reroute is a loss");

    reopen(dev, mix);
    ma_device* const wasapi = A::dev(dev);
    A::self_routing(dev, true);
    A::backend_stops(dev);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::None && A::dev(dev) == wasapi, "a self-routing backend is left alone");
    past_window();
    check(dev.poll() == DeviceEvent::None && A::dev(dev) == wasapi, "a loss left to the backend stays so");

    reopen(dev, mix);
    A::backend_stops(dev);
    A::backend_starts(dev);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::None, "a stop the backend undid itself is not a loss");
    A::notify(dev, ma_device_notification_type_stopped);
    A::notify(dev, ma_device_notification_type_rerouted);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::None, "a stop followed by a reroute is not a loss");

    A::backend_stops(dev);
    A::force_state(dev, ma_device_state_stopping);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::None, "a device in transition is left alone");
    A::force_state(dev, ma_device_state_stopped);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::Restarted, "the device is restarted once the transition ends");

    A::backend_stops(dev);
    wait_out(dev);
    reopen(dev, mix);
    A::backend_stops(dev);
    check(dev.poll() == DeviceEvent::None, "a deliberate stop and start reset the 500 ms window");
    past_window();
    check(dev.poll() == DeviceEvent::Restarted, "a loss after a deliberate stop and start is handled");
    wait_ms(4600);
    A::backend_stops(dev);
    wait_out(dev);
    check(dev.poll() == DeviceEvent::Failed, "the 5 s budget counts to the loss, not to its handling");
    const uint64_t cur = SAMPLE_RATE * 10;
    check(resync_clock(cur + RESYNC_LEAD, cur) == cur + RESYNC_LEAD, "a clock up to the lead is kept");
    check(resync_clock(cur + RESYNC_LEAD + 1, cur) == cur, "a clock past the lead snaps to the cursor");
    if (fails) {
        std::printf("audio-device: FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("audio-device: PASS - 26 cases\n");
    return 0;
}
