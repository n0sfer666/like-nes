#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>

#include "bundle_view.hpp"
#include "mobile_app.hpp"
#include "platform_io.hpp"
#include "quad_batch.hpp"
#include "rumble_credits.hpp"
#include "rumble_flow.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_menu.hpp"
#include "rumble_screen.hpp"
#include "rumble_touch.hpp"
#include "rumble_touch_quads.hpp"
#include "rumble_wave_script.hpp"
#include "surface_frame.hpp"
#include "source.hpp"

namespace rumble {

// Neon Rumble на телефоне: уровень 1 сразу, тач занимает P1 первым нажатием. Явный --demo
// оставляет скрипт для сверки с десктопом, --wave выбирает прохождение волн уровня1. Бандл,
// титры, стек сцен и драка живут от init до
// shutdown; квады с текстурами сцены зависят от формата поверхности и живут от attach до detach.
class MobileRumble final : public mobile::Game {
public:
    bool init(GpuContext& gpu) override;
    bool attach_surface(WGPUSurface surface, uint32_t fb_w, uint32_t fb_h,
                        const mobile::SafeArea& safe) override;
    void detach_surface() override;
    void resize(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) override;
    void pointer(intptr_t id, mobile::Touch phase, float x, float y) override;
    void cancel() override;
    std::string touch_state() const override;
    void frame(double dt) override;
    bool wants_frames() const override;
    void suspend() override;
    void resume() override { lag_ = 0; }
    void shutdown() override;

private:
    void build_targets(uint32_t fb_w, uint32_t fb_h);
    void release_targets();
    bool bind();
    void step();
    void draw();
    void report_surface(const char* what) const;
    bool open_controls();
    void fit_touch(uint32_t w, uint32_t h, const mobile::SafeArea& safe);
    void observe_punch(const Play& play);

    GpuContext* gpu_ = nullptr;
    input::GamepadSource* pads_ = nullptr;
    WGPUSurface surface_ = nullptr;
    SurfaceSpec spec_{};
    platform::MappedFile file_;
    asset::BundleView bundle_;
    Credits credits_;
    Menu menu_;
    // После bundle_, credits_ и menu_: стек хранит ссылки на них.
    Flow flow_{bundle_, credits_, menu_};
    Hotseat hotseat_;
    Touch touch_;
    TouchQuads touch_quads_;
    WaveScript wave_demo_;
    std::unique_ptr<Screen> screen_;
    render::QuadRenderer quads_;
    WGPUColor clear_{};
    double lag_ = 0.0;
    uint32_t ticks_ = 0;
    uint32_t attaches_ = 0;
    uint32_t last_jab_tick_ = 0;
    uint32_t jab_elapsed_ = 0;
    bool was_jab_ = false;
    bool targets_ = false;
    bool lost_ = false;
    bool warned_ = false;
    bool upload_warned_ = false;
    bool inited_ = false;
    bool demo_ = false;
    float touch_scale_ = 1;
};

} // namespace rumble
