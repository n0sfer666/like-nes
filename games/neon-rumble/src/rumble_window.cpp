#include "rumble_window.hpp"

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#include <cstdio>
#include <memory>

#include "gpu.hpp"
#include "haptic.hpp"
#include "quad_batch.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_gpu_frame.hpp"
#include "rumble_feedback.hpp"
#include "rumble_frame_touch.hpp"
#include "rumble_keys.hpp"
#include "rumble_layers.hpp"
#include "rumble_no_pads.hpp"
#include "rumble_screen.hpp"
#include "rumble_shot.hpp"
#include "rumble_window_bench.hpp"
#include "rumble_window_loop.hpp"
#include "source.hpp"
#include "surface_frame.hpp"

namespace rumble {

namespace {

using framework::graphics::ViewportFit;

// Пад без бэкенда — не отказ запуска: двое играют и за одной клавиатурой.
input::GamepadSource* open_pads(GLFWwindow* window, Hotseat& hotseat) {
    input::install_glfw_input(window, hotseat.engine());
    input::GamepadSource* pads = input::make_gamepad_source();
    if (!pads->init()) std::fprintf(stderr, "neon-rumble: pad backend %s did not start\n", pads->backend_name());
    return pads;
}

// Переходы — после `run`: их подаёт код игры по прошедшему тику, а не система сцены.
// docs:begin(flow-keys)
LayerStats step_menu(Flow& flow, Hotseat& hotseat, input::GamepadSource& pads, const WindowKeys& keys, Screen& screen,
                     const Credits& credits, const ViewportFit& fit, uint32_t tick) {
    Menu& menu = flow.menu();
    if (keys.hit(WindowKeys::UP)) menu.move(-1);
    if (keys.hit(WindowKeys::DOWN)) menu.move(1);
    const bool enter = keys.hit(WindowKeys::ENTER);
    if (keys.hit(WindowKeys::CREDITS) || (enter && menu.item == MenuItem::Credits)) ++screen.shown;
    pads.poll(hotseat.engine());
    hotseat.engine().drain();
    flow.run(tick);
    if (enter && menu.item != MenuItem::Credits) flow.enter();
    return screen.menu(menu, credits, fit, tick);
}

LayerStats step_level(Flow& flow, Play& play, Hotseat& hotseat, input::GamepadSource& pads, const WindowKeys& keys,
                      Screen& screen, const Credits& credits, const ViewportFit& fit, uint32_t tick, bool feedback) {
    if (keys.hit(WindowKeys::OVERLAY)) screen.overlay = !screen.overlay;
    if (keys.hit(WindowKeys::CREDITS)) ++screen.shown;
    pads.poll(hotseat.engine());
    if (hotseat.tick(play.scene.ticks + 1, play.brawl.players)) {
        flow.run(tick);
        if (feedback) hit_feedback(play.brawl, hotseat);
        report_step(play.brawl, play.scene.ticks);
    }
    if (keys.hit(WindowKeys::ESCAPE)) flow.leave();
    return screen.level(play, hotseat, credits, fit, tick);
}
// docs:end(flow-keys)


} // namespace

int window_frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Flow& flow,
                      Hotseat& hotseat, const Credits& credits, const WindowRun& run, int& drawn) {
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    SurfaceSpec spec{surface, WGPUTextureFormat_Undefined, static_cast<uint32_t>(fbw),
                     static_cast<uint32_t>(fbh)};
    spec.format = configure_surface(surface, gpu.adapter, gpu.device, spec.width, spec.height);
    render::QuadRenderer quads;
    WGPUColor clear{};
    if (!bind_scene(quads, gpu, spec.format, flow, credits, clear)) {
        std::fprintf(stderr, "neon-rumble: quad renderer or menu texture upload failed\n");
        return 1;
    }
    auto screen = std::make_unique<Screen>();
    NoPads no_pads;
    auto* bench = run.benchmark;
    input::GamepadSource* pads = run.script || bench ? &no_pads : open_pads(window, hotseat);
    if (!run.script && !bench) haptic::init();
    WindowKeys keys;
    bool cropped = false;
    bool warned = false;
    bool upload_warned = false;
    bool shots_ok = true;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!run.script && !bench) keys.poll(window);
        if (bench && glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            bench->frames.suspend();
            glfwWaitEvents();
            continue;
        }
        if (bench && bench->frames.paused()) bench->frames.resume();
        int nw = 0, nh = 0;
        glfwGetFramebufferSize(window, &nw, &nh);
        if (nw > 0 && nh > 0 && (static_cast<uint32_t>(nw) != spec.width
                                 || static_cast<uint32_t>(nh) != spec.height)) {
            spec.width = static_cast<uint32_t>(nw);
            spec.height = static_cast<uint32_t>(nh);
            reconfigure_surface(spec, gpu.device);
        }
        const auto begin = bench ? FrameClock::now() : FrameClock::time_point{};
        if (bench) bench->frames.poll();
        const SurfaceFrame sf = acquire_frame(spec, gpu, "neon-rumble", warned);
        if (sf.quit) return 1;
        // Пропущенный кадр всё равно разбирает очередь: GLFW пишет в неё из колбэков, и переполнение
        // съело бы отпускание клавиши — блок залип бы без следа.
        if (!sf.texture) {
            hotseat.engine().drain();
            continue;
        }
        if (bench && bench->frames.presented() == FrameStatsRun::MENU_FRAMES && !flow.play()) flow.enter();
        if (flow.apply()) {
            screen->reset();
            if (!bind_scene(quads, gpu, spec.format, flow, credits, clear)) {
                std::fprintf(stderr, "neon-rumble: quad renderer or scene texture upload failed\n");
                wgpuTextureRelease(sf.texture);
                return 1;
            }
            if (run.script && flow.play() != nullptr) run.script->enter_level();
        }
        const auto tick = static_cast<uint32_t>(drawn);
        const ViewportFit fit = framework::graphics::viewport_fit(
            {spec.width, spec.height}, flow.viewport().zone, flow.viewport().limit);
        if (fit.cropped && !cropped)
            std::fprintf(stderr, "neon-rumble: window %ux%u is smaller than the %ux%u zone, the zone is cropped\n",
                         spec.width, spec.height, flow.viewport().zone.w, flow.viewport().zone.h);
        cropped = fit.cropped;
        Play* play = flow.play();
        const uint32_t level_tick = play ? play->scene.ticks : 0;
        if (bench) {
            bench->frames.tick();
            if (play) bench->feed(spec.width, spec.height, level_tick);
        }
        if (run.script)
            run.script->feed(play != nullptr, play != nullptr ? play->scene.ticks + 1 : tick, hotseat.engine(), keys);
        LayerStats st = play != nullptr
            ? step_level(flow, *play, hotseat, *pads, keys, *screen, credits, fit, tick, !run.script && !bench)
            : step_menu(flow, hotseat, *pads, keys, *screen, credits, fit, tick);
        keys.consume();
        const auto scene_runs = st.runs;
        if (bench && play) {
            if (play->scene.ticks != level_tick) observe_frame_touch(bench->frames, hotseat, *play);
            bench->overlay(*play, credits, screen->layers(), st);
        }
        const bool uploaded = quads.upload(screen->layers().quads(st), spec.width, spec.height);
        if (!uploaded && !upload_warned) {
            std::fprintf(stderr, "neon-rumble: %u quad(s) not uploaded, frame left empty\n", st.quads);
            upload_warned = true;
        }
        if (run.script && !shoot_due(gpu, ShotTarget{spec.format, spec.width, spec.height, clear, fit.shown}, quads,
                                     screen->layers().runs(st), run, play != nullptr ? play->scene.ticks : 0, tick))
            shots_ok = false;
        if (run.script && run.script->quit() && play != nullptr) report_brawl(play->brawl, play->scene.ticks);
        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        const bool submitted = view != nullptr;
        if (submitted) {
            const auto runs = screen->layers().runs(st);
            draw_frame(gpu, surface, view, clear, fit.shown, quads, runs.first(scene_runs),
                       QuadOverlay{{0, 0, spec.width, spec.height}, runs.subspan(scene_runs)});
            wgpuTextureViewRelease(view);
        }
        wgpuTextureRelease(sf.texture);
        if (bench && submitted && uploaded && !gpu.device_lost.load())
            bench->frames.present(begin, FrameClock::now(), play == nullptr);
        if (++drawn == run.frames || (run.script && run.script->quit()) || (bench && bench->frames.ready()))
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (run.script && !run.script->quit()) {
        std::fprintf(stderr, "neon-rumble: the window closed at frame %d before the script's 'quit'\n", drawn);
        return 1;
    }
    return shots_ok ? 0 : 1;
}

} // namespace rumble
