#include "rumble.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cstdio>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "rumble_banner.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_gpu_frame.hpp"
#include "rumble_keys.hpp"
#include "rumble_layers.hpp"
#include "rumble_roster_quads.hpp"
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

int frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Scene& scene, Hotseat& hotseat,
               const Level& level, const Fighters& fighters, const Credits& credits, int frames, int& drawn) {
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    SurfaceSpec spec{surface, WGPUTextureFormat_Undefined, static_cast<uint32_t>(fbw),
                     static_cast<uint32_t>(fbh)};
    spec.format = configure_surface(surface, gpu.adapter, gpu.device, spec.width, spec.height);
    render::QuadRenderer quads;
    if (!init_quads(quads, gpu, spec.format, level, fighters, credits)) {
        std::fprintf(stderr, "neon-rumble: quad renderer or level texture upload failed\n");
        return 1;
    }
    Layers layers;
    FighterQuads fighter_quads;
    CreditsQuads credits_quads;
    BannerQuads banner;
    input::GamepadSource* pads = open_pads(window, hotseat);
    const WGPUColor clear = background(level, spec.format);
    bool cropped = false;
    bool overlay = false;
    bool f3_held = false;
    uint32_t shown = 0;
    bool f1_held = false;
    bool warned = false;
    bool upload_warned = false;
    bool fighter_warned = false;
    bool credits_warned = false;
    // Пока на улице никого, камера держит последнюю точку: без неё слои ушли бы в автопрогон.
    framework::Vec2 follow;
    bool following = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (key_toggled(window, GLFW_KEY_F3, f3_held)) overlay = !overlay;
        if (key_toggled(window, GLFW_KEY_F1, f1_held)) ++shown;
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
        pads->poll(hotseat.engine());
        if (hotseat.tick(scene.ticks + 1, scene.brawl->players)) {
            scene.step(static_cast<uint32_t>(drawn));
            report_step(*scene.brawl, scene.ticks);
        }
        const auto tick = static_cast<uint64_t>(drawn);
        const ViewportFit fit = framework::graphics::viewport_fit({spec.width, spec.height});
        if (fit.cropped && !cropped)
            std::fprintf(stderr, "neon-rumble: window %ux%u is smaller than the %ux%u zone, the zone is cropped\n",
                         spec.width, spec.height, framework::graphics::VIEW_ZONE.w, framework::graphics::VIEW_ZONE.h);
        cropped = fit.cropped;
        if (follow_point(*scene.brawl, follow)) following = true;
        LayerStats st = layers.build(level, fit, tick, following ? &follow : nullptr);
        const FighterStats fs = draw_roster(fighter_quads, fighters, *scene.brawl, layers, st, level.texture_count,
                                            overlay);
        banner.add(credits, banner_text(hotseat.lobby()), fit, layers, st, atlas_texture(level.texture_count),
                   solid_texture(level.texture_count));
        if (fs.rejected + fs.dropped > 0 && !fighter_warned) {
            std::fprintf(stderr, "neon-rumble: tick %llu: fighters %u quad(s) rejected, %u dropped\n",
                         static_cast<unsigned long long>(tick), fs.rejected, fs.dropped);
            fighter_warned = true;
        }
        const CreditStats cs = shown > 0 ? credits_quads.add(credits, shown - 1, fit, layers, st,
                                                             atlas_texture(level.texture_count),
                                                             solid_texture(level.texture_count))
                                         : CreditStats{};
        if (shown > cs.pages) shown = 0;
        if (cs.unknown + cs.dropped > 0 && !credits_warned) {
            std::fprintf(stderr, "neon-rumble: tick %llu: credits %u unknown glyph(s), %u dropped\n",
                         static_cast<unsigned long long>(tick), cs.unknown, cs.dropped);
            credits_warned = true;
        }
        if (!quads.upload(layers.quads(st), spec.width, spec.height) && !upload_warned) {
            std::fprintf(stderr, "neon-rumble: %u quad(s) not uploaded, frame left empty\n", st.quads);
            upload_warned = true;
        }
        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        draw_frame(gpu, surface, view, clear, fit.shown, quads, layers.runs(st));
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(sf.texture);
        if (++drawn == frames) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    return 0;
}

} // namespace

int run_window(Scene& scene, Hotseat& hotseat, const Level& level, const Fighters& fighters, const Credits& credits, int frames) {
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
        rc = frame_loop(window, gpu, surface, scene, hotseat, level, fighters, credits, frames, drawn);
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
