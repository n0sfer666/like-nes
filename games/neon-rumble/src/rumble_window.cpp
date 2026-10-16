#include "rumble_window.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cstdio>
#include <memory>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_gpu_frame.hpp"
#include "rumble_keys.hpp"
#include "rumble_layers.hpp"
#include "rumble_screen.hpp"
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

int frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Flow& flow, Hotseat& hotseat,
               const Credits& credits, int frames, int& drawn) {
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
    input::GamepadSource* pads = open_pads(window, hotseat);
    WindowKeys keys;
    bool cropped = false;
    bool warned = false;
    bool upload_warned = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        keys.poll(window);
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
        }
        const auto tick = static_cast<uint32_t>(drawn);
        const ViewportFit fit = framework::graphics::viewport_fit({spec.width, spec.height});
        if (fit.cropped && !cropped)
            std::fprintf(stderr, "neon-rumble: window %ux%u is smaller than the %ux%u zone, the zone is cropped\n",
                         spec.width, spec.height, framework::graphics::VIEW_ZONE.w, framework::graphics::VIEW_ZONE.h);
        cropped = fit.cropped;
        Play* play = flow.play();
        const LayerStats st = play != nullptr
                                  ? step_level(flow, *play, hotseat, *pads, keys, *screen, credits, fit, tick)
                                  : step_menu(flow, hotseat, *pads, keys, *screen, credits, fit, tick);
        keys.consume();
        if (!quads.upload(screen->layers().quads(st), spec.width, spec.height) && !upload_warned) {
            std::fprintf(stderr, "neon-rumble: %u quad(s) not uploaded, frame left empty\n", st.quads);
            upload_warned = true;
        }
        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        draw_frame(gpu, surface, view, clear, fit.shown, quads, screen->layers().runs(st));
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(sf.texture);
        if (++drawn == frames) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    return 0;
}

} // namespace

int run_window(Flow& flow, Hotseat& hotseat, const Credits& credits, int frames) {
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
        rc = frame_loop(window, gpu, surface, flow, hotseat, credits, frames, drawn);
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
