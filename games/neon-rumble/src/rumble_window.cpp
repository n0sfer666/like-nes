#include "rumble_window.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cstdio>
#include <memory>
#include <string>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_gpu_frame.hpp"
#include "rumble_keys.hpp"
#include "rumble_layers.hpp"
#include "rumble_no_pads.hpp"
#include "rumble_screen.hpp"
#include "rumble_shot.hpp"
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
                      Screen& screen, const Credits& credits, const ViewportFit& fit, uint32_t tick) {
    if (keys.hit(WindowKeys::OVERLAY)) screen.overlay = !screen.overlay;
    if (keys.hit(WindowKeys::CREDITS)) ++screen.shown;
    pads.poll(hotseat.engine());
    if (hotseat.tick(play.scene.ticks + 1, play.brawl.players)) {
        flow.run(tick);
        report_step(play.brawl, play.scene.ticks);
    }
    if (keys.hit(WindowKeys::ESCAPE)) flow.leave();
    return screen.level(play, hotseat, credits, fit, tick);
}
// docs:end(flow-keys)

bool shoot_due(const GpuContext& gpu, const ShotTarget& target, const render::QuadRenderer& quads,
               std::span<const render::QuadRun> runs, const WindowRun& run, uint32_t tick, uint32_t frame) {
    bool ok = true;
    for (const std::string& name : run.script->shots()) {
        const std::string path = std::string(run.shots) + "/" + name + ".png";
        if (shoot(gpu, target, quads, runs, path)) {
            std::printf("shot %s frame %u tick %u\n", name.c_str(), frame, tick);
        } else {
            std::fprintf(stderr, "neon-rumble: shot %s not written to %s\n", name.c_str(), path.c_str());
            ok = false;
        }
    }
    return ok;
}

int frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Flow& flow, Hotseat& hotseat,
               const Credits& credits, const WindowRun& run, int& drawn) {
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
    input::GamepadSource* pads = run.script ? &no_pads : open_pads(window, hotseat);
    WindowKeys keys;
    bool cropped = false;
    bool warned = false;
    bool upload_warned = false;
    bool shots_ok = true;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!run.script) keys.poll(window);
        int nw = 0, nh = 0;
        glfwGetFramebufferSize(window, &nw, &nh);
        if (nw > 0 && nh > 0 && (static_cast<uint32_t>(nw) != spec.width
                                 || static_cast<uint32_t>(nh) != spec.height)) {
            spec.width = static_cast<uint32_t>(nw);
            spec.height = static_cast<uint32_t>(nh);
            reconfigure_surface(spec, gpu.device);
        }
        const SurfaceFrame sf = acquire_frame(spec, gpu, "neon-rumble", warned);
        if (sf.quit) return 1;
        // Пропущенный кадр всё равно разбирает очередь: GLFW пишет в неё из колбэков, и переполнение
        // съело бы отпускание клавиши — блок залип бы без следа.
        if (!sf.texture) {
            hotseat.engine().drain();
            continue;
        }
        if (flow.apply()) {
            screen->reset();
            if (!bind_scene(quads, gpu, spec.format, flow, credits, clear)) {
                std::fprintf(stderr, "neon-rumble: quad renderer or scene texture upload failed\n");
                return 1;
            }
            if (run.script && flow.play() != nullptr) run.script->enter_level();
        }
        const auto tick = static_cast<uint32_t>(drawn);
        const ViewportFit fit = framework::graphics::viewport_fit({spec.width, spec.height});
        if (fit.cropped && !cropped)
            std::fprintf(stderr, "neon-rumble: window %ux%u is smaller than the %ux%u zone, the zone is cropped\n",
                         spec.width, spec.height, framework::graphics::VIEW_ZONE.w, framework::graphics::VIEW_ZONE.h);
        cropped = fit.cropped;
        Play* play = flow.play();
        if (run.script)
            run.script->feed(play != nullptr, play != nullptr ? play->scene.ticks + 1 : tick, hotseat.engine(), keys);
        const LayerStats st = play != nullptr
                                  ? step_level(flow, *play, hotseat, *pads, keys, *screen, credits, fit, tick)
                                  : step_menu(flow, hotseat, *pads, keys, *screen, credits, fit, tick);
        keys.consume();
        if (!quads.upload(screen->layers().quads(st), spec.width, spec.height) && !upload_warned) {
            std::fprintf(stderr, "neon-rumble: %u quad(s) not uploaded, frame left empty\n", st.quads);
            upload_warned = true;
        }
        if (run.script && !shoot_due(gpu, ShotTarget{spec.format, spec.width, spec.height, clear, fit.shown}, quads,
                                     screen->layers().runs(st), run, play != nullptr ? play->scene.ticks : 0, tick))
            shots_ok = false;
        if (run.script && run.script->quit() && play != nullptr) report_brawl(play->brawl, play->scene.ticks);
        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        draw_frame(gpu, surface, view, clear, fit.shown, quads, screen->layers().runs(st));
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(sf.texture);
        if (++drawn == run.frames || (run.script && run.script->quit())) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (run.script && !run.script->quit()) {
        std::fprintf(stderr, "neon-rumble: the window closed at frame %d before the script's 'quit'\n", drawn);
        return 1;
    }
    return shots_ok ? 0 : 1;
}

} // namespace

int run_window(Flow& flow, Hotseat& hotseat, const Credits& credits, const WindowRun& run) {
    if (!glfwInit()) {
        std::fprintf(stderr, "neon-rumble: glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(960, 540, "Neon Rumble", nullptr, nullptr);
    GpuContext gpu;
    WGPUSurface surface = nullptr;
    int rc = 1;
    int drawn = 0;
    if (!window) {
        std::fprintf(stderr, "neon-rumble: window creation failed\n");
    } else if (!(gpu.instance = gpu.create_instance())) {
        std::fprintf(stderr, "neon-rumble: webgpu instance failed\n");
    } else if (!(surface = glfwGetWGPUSurface(gpu.instance, window)) || !gpu.init(surface)) {
        std::fprintf(stderr, "neon-rumble: no surface, adapter or device\n");
    } else {
        rc = frame_loop(window, gpu, surface, flow, hotseat, credits, run, drawn);
        if (rc == 0 && drawn == 0) {
            std::fprintf(stderr, "neon-rumble: window closed before the first frame\n");
            rc = 1;
        }
    }
    if (surface) wgpuSurfaceRelease(surface);
    gpu.shutdown();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
    if (rc == 0) std::printf("neon-rumble: window run ok, %d frames\n", drawn);
    return rc;
}

} // namespace rumble
