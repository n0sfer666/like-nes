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
#include "surface_frame.hpp"

namespace rumble {

// Neon Rumble на телефоне: уровень 1 сразу, без меню, P1 и P2 играют скрипт хотсита. Касания и пады
// не читаются — сенсорное управление приходит с В2, пады — с В3. Бандл, титры, стек сцен и драка живут от init до
// shutdown; квады с текстурами сцены зависят от формата поверхности и живут от attach до detach.
class MobileRumble final : public mobile::Game {
public:
    bool init(GpuContext& gpu) override;
    bool attach_surface(WGPUSurface surface, uint32_t fb_w, uint32_t fb_h,
                        const mobile::SafeArea& safe) override;
    void detach_surface() override;
    void resize(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) override;
    void pointer(intptr_t, mobile::Touch, float, float) override {}
    void cancel() override {}
    void frame(double dt) override;
    bool wants_frames() const override;
    void suspend() override {}
    void resume() override {}
    void shutdown() override;

private:
    void build_targets(uint32_t fb_w, uint32_t fb_h);
    void release_targets();
    bool bind();
    void step();
    void draw();
    void report_surface(const char* what) const;

    GpuContext* gpu_ = nullptr;
    WGPUSurface surface_ = nullptr;
    SurfaceSpec spec_{};
    platform::MappedFile file_;
    asset::BundleView bundle_;
    Credits credits_;
    Menu menu_;
    // После bundle_, credits_ и menu_: стек хранит ссылки на них.
    Flow flow_{bundle_, credits_, menu_};
    Hotseat hotseat_;
    std::unique_ptr<Screen> screen_;
    render::QuadRenderer quads_;
    WGPUColor clear_{};
    double lag_ = 0.0;
    uint32_t ticks_ = 0;
    uint32_t attaches_ = 0;
    bool targets_ = false;
    bool lost_ = false;
    bool warned_ = false;
    bool upload_warned_ = false;
    bool inited_ = false;
};

} // namespace rumble
