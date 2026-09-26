#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

// AudioDevice HAL (спека #3): реальный вывод через miniaudio (WASAPI/CoreAudio/ALSA/Pulse +
// mobile). Console/mobile-бэкенды — точки расширения. RT-поток miniaudio зовёт RenderFn;
// в CI устройство не стартуется (deviceless-гейты гоняют mixer напрямую).
namespace audio {

using RenderFn = void (*)(void* user, uint32_t frames, int16_t* out);

// Что случилось с выводом с прошлого опроса (аудит #21 A·3·5).
enum class DeviceEvent { None, Restarted, Failed };

class MiniaudioDevice {
public:
    ~MiniaudioDevice();
    // null_backend: тот же ma_device и тот же RT-поток, но без железа — машинный суррогат
    // «живого звука» для CI (на раннерах аудиоустройства нет). Сенсорика — на железе владельца.
    bool start(RenderFn fn, void* user, bool null_backend = false);
    void stop();
    // Поток игры, раз в тик (аудит #21 A·3·5). Вывод, который бэкенд остановил и за 500 мс тишины
    // в уведомлениях не поднял сам, перезапускается с теми же аргументами; вторая потеря в пределах
    // 5 с после перезапуска — тишина до конца сессии без новых попыток. WASAPI не перезапускается:
    // miniaudio сам переводит и поднимает там поток, серией по разу на роль вывода, а uninit
    // посреди долгого reroute (Bluetooth) гонится с его COM-потоком. Каждая развязка печатается в
    // stderr отсюда, из потока игры: из коллбэка уведомлений miniaudio запрещает трогать устройство.
    DeviceEvent poll();
    uint32_t sample_rate() const { return rate_; }

private:
    friend struct DeviceTestAccess;
    bool open();
    void close();
    bool lost(void* device) const;

    RenderFn fn_ = nullptr;
    void* user_ = nullptr;
    bool null_backend_ = false;
    std::atomic<bool> lost_{false};
    std::atomic<bool> rerouted_{false};
    std::atomic<uint32_t> epoch_{0};
    uint32_t seen_epoch_ = 0;
    uint32_t reported_epoch_ = UINT32_MAX;
    bool self_routing_ = false;
    bool lost_seen_ = false;
    bool restarted_ = false;
    std::chrono::steady_clock::time_point lost_since_{}, restarted_at_{};
    void* device_ = nullptr;  // ma_device*
    void* context_ = nullptr; // ma_context* (только для null_backend)
    uint32_t rate_ = 0;
};

} // namespace audio
