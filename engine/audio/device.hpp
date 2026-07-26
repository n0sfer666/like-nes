#pragma once
#include <cstdint>

// AudioDevice HAL (спека #3): реальный вывод через miniaudio (WASAPI/CoreAudio/ALSA/Pulse +
// mobile). Console/mobile-бэкенды — точки расширения. RT-поток miniaudio зовёт RenderFn;
// в CI устройство не стартуется (deviceless-гейты гоняют mixer напрямую).
namespace audio {

using RenderFn = void (*)(void* user, uint32_t frames, int16_t* out);

class MiniaudioDevice {
public:
    ~MiniaudioDevice();
    // null_backend: тот же ma_device и тот же RT-поток, но без железа — машинный суррогат
    // «живого звука» для CI (на раннерах аудиоустройства нет). Сенсорика — на железе владельца.
    bool start(RenderFn fn, void* user, bool null_backend = false);
    void stop();
    uint32_t sample_rate() const { return rate_; }

private:
    void* device_ = nullptr;  // ma_device*
    void* context_ = nullptr; // ma_context* (только для null_backend)
    uint32_t rate_ = 0;
};

} // namespace audio
