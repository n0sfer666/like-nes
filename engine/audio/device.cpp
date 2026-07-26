#include "device.hpp"

#include <cstdlib>

#include "audio_types.hpp"
#include "miniaudio.h"

namespace audio {
namespace {

struct Trampoline {
    RenderFn fn;
    void* user;
};

void render_trampoline(ma_device* dev, void* output, const void* /*input*/, ma_uint32 frame_count) {
    auto* t = static_cast<Trampoline*>(dev->pUserData);
    t->fn(t->user, static_cast<uint32_t>(frame_count), static_cast<int16_t*>(output));
}

} // namespace

MiniaudioDevice::~MiniaudioDevice() { stop(); }

bool MiniaudioDevice::start(RenderFn fn, void* user, bool null_backend) {
    if (device_) return false;
    auto* t = new Trampoline{fn, user};
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_s16;
    cfg.playback.channels = OUT_CHANNELS;
    cfg.sampleRate = SAMPLE_RATE;
    cfg.dataCallback = render_trampoline;
    cfg.pUserData = t;

    ma_context* ctx = nullptr;
    if (null_backend) {
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
    if (!dev) { stop(); delete t; return false; }
    if (ma_device_init(ctx, &cfg, dev) != MA_SUCCESS) {
        std::free(dev);
        stop();
        delete t;
        return false;
    }
    device_ = dev;
    rate_ = dev->sampleRate;
    if (ma_device_start(dev) != MA_SUCCESS) {
        stop();
        return false;
    }
    return true;
}

void MiniaudioDevice::stop() {
    if (device_) {
        auto* dev = static_cast<ma_device*>(device_);
        Trampoline* t = static_cast<Trampoline*>(dev->pUserData);
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

} // namespace audio
