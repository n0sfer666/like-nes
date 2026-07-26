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

bool MiniaudioDevice::start(RenderFn fn, void* user) {
    if (device_) return false;
    auto* t = new Trampoline{fn, user};
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_s16;
    cfg.playback.channels = OUT_CHANNELS;
    cfg.sampleRate = SAMPLE_RATE;
    cfg.dataCallback = render_trampoline;
    cfg.pUserData = t;
    auto* dev = static_cast<ma_device*>(std::malloc(sizeof(ma_device)));
    if (!dev) { delete t; return false; }
    if (ma_device_init(nullptr, &cfg, dev) != MA_SUCCESS) {
        std::free(dev);
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
    if (!device_) return;
    auto* dev = static_cast<ma_device*>(device_);
    Trampoline* t = static_cast<Trampoline*>(dev->pUserData);
    ma_device_uninit(dev);
    std::free(dev);
    delete t;
    device_ = nullptr;
}

} // namespace audio
