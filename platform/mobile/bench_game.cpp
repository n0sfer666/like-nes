#include <cstdio>
#include <string>
#include <thread>

#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

#include "mobile_app.hpp"
#include "mobile_host.hpp"
#include "resim_bench.hpp"

namespace {

// Бенч ресима глазами оболочки — та же игра, что и шутер, только без кадра: счёт идёт в своём
// потоке с первого init, а поверхность ему не нужна. Тем же входом, а не своим main: бенч на
// телефоне судит и оболочку, на которой поедет игра, а не отдельную, которую игра не видит.
class BenchGame final : public mobile::Game {
public:
    bool init(GpuContext&) override {
        if (worker_.joinable()) return true;
        std::printf("[bench] %s\n", mobile::device_info().c_str());
        std::fflush(stdout);
        worker_ = std::thread([against = mobile::asset_path("crossplay.txt")] {
#if defined(__APPLE__)
            // Поток по умолчанию iOS ставит ниже интерактивного: бюджет мерил бы планировщик.
            pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
            framework::ai::bench::run_resim_bench(against);
            std::fflush(stdout);
        });
        return true;
    }
    bool attach_surface(WGPUSurface, uint32_t, uint32_t, const mobile::SafeArea&) override { return true; }
    void detach_surface() override {}
    void resize(uint32_t, uint32_t, const mobile::SafeArea&) override {}
    void pointer(intptr_t, mobile::Touch, float, float) override {}
    void cancel() override {}
    void frame(double) override {}
    bool wants_frames() const override { return false; }
    void suspend() override {}
    void resume() override {}
    void shutdown() override {
        if (worker_.joinable()) worker_.join();
    }
    ~BenchGame() override { shutdown(); }

private:
    std::thread worker_;
};

} // namespace

std::unique_ptr<mobile::Game> mobile::make_game() { return std::make_unique<BenchGame>(); }
