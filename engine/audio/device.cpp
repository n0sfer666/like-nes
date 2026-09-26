#include "device.hpp"

#include <cstdio>
#include <cstdlib>

#include "audio_types.hpp"
#include "miniaudio.h"

namespace audio {
namespace {

constexpr auto GRACE = std::chrono::milliseconds(500);
constexpr auto BUDGET = std::chrono::seconds(5);

struct Trampoline {
    RenderFn fn;
    void* user;
    std::atomic<bool>* lost;
    std::atomic<bool>* rerouted;
    std::atomic<uint32_t>* epoch;
};

void render_trampoline(ma_device* dev, void* output, const void* /*input*/, ma_uint32 frame_count) {
    auto* t = static_cast<Trampoline*>(dev->pUserData);
    t->fn(t->user, static_cast<uint32_t>(frame_count), static_cast<int16_t*>(output));
}

// Только флаги: `rerouted` приходит, когда бэкенд уже сам перевёл поток, а `started` — когда он
// поднял его обратно, и оба снимают подозрение с предшествующего `stopped`. Эпоха растёт на каждом
// уведомлении: серия reroute WASAPI не должна складываться в одно окно ожидания.
void on_notification(const ma_device_notification* n) {
    auto* t = static_cast<Trampoline*>(n->pDevice->pUserData);
    t->epoch->fetch_add(1);
    switch (n->type) {
    case ma_device_notification_type_stopped: t->lost->store(true); break;
    case ma_device_notification_type_started: t->lost->store(false); break;
    case ma_device_notification_type_rerouted:
        t->lost->store(false);
        t->rerouted->store(true);
        break;
    default: break;
    }
}

// Pulse при suspend шлёт `stopped`, оставляя состояние `started`, а синхронный рабочий поток шлёт
// его ещё в `stopping`, поэтому ждать приходится только переходов: uninit посреди них гонится с
// рабочим потоком.
bool settling(ma_device* dev) {
    const ma_device_state st = ma_device_get_state(dev);
    return st == ma_device_state_starting || st == ma_device_state_stopping;
}

} // namespace

MiniaudioDevice::~MiniaudioDevice() { stop(); }

bool MiniaudioDevice::start(RenderFn fn, void* user, bool null_backend) {
    if (device_) return false;
    fn_ = fn;
    user_ = user;
    null_backend_ = null_backend;
    lost_seen_ = false;
    restarted_ = false;
    return open();
}

bool MiniaudioDevice::open() {
    auto* t = new Trampoline{fn_, user_, &lost_, &rerouted_, &epoch_};
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_s16;
    cfg.playback.channels = OUT_CHANNELS;
    cfg.sampleRate = SAMPLE_RATE;
    cfg.dataCallback = render_trampoline;
    cfg.notificationCallback = on_notification;
    cfg.pUserData = t;

    ma_context* ctx = nullptr;
    if (null_backend_) {
        ctx = static_cast<ma_context*>(std::malloc(sizeof(ma_context)));
        if (!ctx) { delete t; return false; }
        ma_backend backend = ma_backend_null;
        if (ma_context_init(&backend, 1, nullptr, ctx) != MA_SUCCESS) {
            std::free(ctx);
            delete t;
            return false;
        }
        context_ = ctx;
    }

    auto* dev = static_cast<ma_device*>(std::malloc(sizeof(ma_device)));
    if (!dev) { close(); delete t; return false; }
    if (ma_device_init(ctx, &cfg, dev) != MA_SUCCESS) {
        std::free(dev);
        close();
        delete t;
        return false;
    }
    device_ = dev;
    rate_ = dev->sampleRate;
    self_routing_ = dev->pContext->backend == ma_backend_wasapi;
    if (ma_device_start(dev) != MA_SUCCESS) {
        close();
        return false;
    }
    return true;
}

// ma_device_uninit не останавливает устройство и не будит рабочий поток: приостановленный Pulse
// ждёт в pa_mainloop_iterate события сервера, и uninit стоял бы вместе с потоком игры до resume.
// Будит его только ma_device_stop.
void MiniaudioDevice::close() {
    if (device_) {
        auto* dev = static_cast<ma_device*>(device_);
        Trampoline* t = static_cast<Trampoline*>(dev->pUserData);
        if (ma_device_get_state(dev) == ma_device_state_started) ma_device_stop(dev);
        ma_device_uninit(dev);
        std::free(dev);
        delete t;
        device_ = nullptr;
    }
    if (context_) {
        auto* ctx = static_cast<ma_context*>(context_);
        ma_context_uninit(ctx);
        std::free(ctx);
        context_ = nullptr;
    }
}

void MiniaudioDevice::stop() { close(); }

// Вне WASAPI потеря — и устройство, стоящее без уведомления: обёртка сама его не останавливает, а
// miniaudio молчит, когда остановка при выдёргивании не удалась, и после reroute CoreAudio с
// несостоявшимся стартом шлёт только `rerouted`.
bool MiniaudioDevice::lost(void* device) const {
    auto* dev = static_cast<ma_device*>(device);
    return lost_.load() || (!self_routing_ && ma_device_get_state(dev) == ma_device_state_stopped);
}

DeviceEvent MiniaudioDevice::poll() {
    if (rerouted_.exchange(false)) std::fprintf(stderr, "[audio] output rerouted by the backend\n");
    auto* dev = static_cast<ma_device*>(device_);
    const uint32_t epoch = epoch_.load();
    const bool notified = epoch != seen_epoch_;
    seen_epoch_ = epoch;
    if (!dev || !lost(dev) || settling(dev)) {
        lost_seen_ = false;
        return DeviceEvent::None;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!lost_seen_ || notified) {
        lost_seen_ = true;
        lost_since_ = now;
    }
    if (now - lost_since_ < GRACE) return DeviceEvent::None;
    lost_seen_ = false;
    if (self_routing_) {
        // Флаг не снимается: уведомление, пришедшее между проверкой и сбросом, пропало бы. Строка
        // одна на эпоху — новая потеря сдвигает эпоху и получает свою.
        if (reported_epoch_ != epoch)
            std::fprintf(stderr, "[audio] output device stopped - left to the backend to bring back\n");
        reported_epoch_ = epoch;
        return DeviceEvent::None;
    }
    lost_ = false;
    close();
    if (restarted_ && lost_since_ - restarted_at_ < BUDGET) {
        std::fprintf(stderr, "[audio] output device stopped again within 5 s of a restart - no sound until the game restarts\n");
        return DeviceEvent::Failed;
    }
    if (!open()) {
        std::fprintf(stderr, "[audio] output device stopped and did not come back - no sound until the game restarts\n");
        return DeviceEvent::Failed;
    }
    restarted_ = true;
    restarted_at_ = now;
    std::fprintf(stderr, "[audio] output device stopped - restarted\n");
    return DeviceEvent::Restarted;
}

} // namespace audio
