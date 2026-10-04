#include "rumble.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cmath>
#include <cstdio>
#include <span>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_layers.hpp"
#include "surface_frame.hpp"

namespace rumble {

namespace {

using framework::graphics::PixelRect;
using framework::graphics::ViewportFit;

double channel(uint32_t rgba, int shift, bool srgb) {
    const double v = static_cast<double>((rgba >> shift) & 0xffu) / 255.0;
    if (!srgb) return v;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

WGPUColor background(const Level& level, WGPUTextureFormat format) {
    const uint32_t rgba = level.map.row->background_rgba;
    const bool srgb = render::quad_target_srgb(format);
    return {channel(rgba, 24, srgb), channel(rgba, 16, srgb), channel(rgba, 8, srgb), 1.0};
}

void draw_frame(const GpuContext& gpu, WGPUSurface surface, WGPUTextureView view, WGPUColor clear,
                const PixelRect& shown, const render::QuadRenderer& quads,
                std::span<const render::QuadRun> runs) {
    WGPURenderPassColorAttachment color = {};
    color.view = view;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = clear;

    WGPURenderPassDescriptor rp = {};
    rp.label = "neon-rumble-layers";
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &color;

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    wgpuRenderPassEncoderSetScissorRect(pass, static_cast<uint32_t>(shown.x), static_cast<uint32_t>(shown.y),
                                        shown.w, shown.h);
    quads.draw(pass, runs);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuSurfacePresent(surface);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

// Текстуры уровня уходят в GPU один раз, в порядке `Level::guids`: этот порядок и есть номер
// материала спрайта, который `draw_layer` выдаёт по guid. Лист бойца и сплошная текстура оверлея
// встают сразу за ними — их номера `texture_count` и `texture_count + 1`.
bool init_quads(render::QuadRenderer& quads, const GpuContext& gpu, WGPUTextureFormat format,
                const Level& level, const Fighter& fighter) {
    static constexpr uint8_t SOLID[4] = {255, 255, 255, 255};
    if (!quads.init(gpu.device, gpu.queue, format, Layers::CAPACITY)) return false;
    for (uint32_t i = 0; i < level.texture_count; ++i)
        if (!quads.add_texture(level.pixels[i].pixels, level.pixels[i].width,
                               level.pixels[i].height))
            return false;
    return quads.add_texture(fighter.sheet.pixels, fighter.sheet.width, fighter.sheet.height)
           && quads.add_texture(SOLID, 1, 1);
}

bool overlay_toggled(GLFWwindow* window, bool& held) {
    const bool down = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
    const bool pressed = down && !held;
    held = down;
    return pressed;
}

int frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Scene& scene,
               const Level& level, const Fighter& fighter, int frames, int& drawn) {
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    SurfaceSpec spec{surface, WGPUTextureFormat_Undefined, static_cast<uint32_t>(fbw),
                     static_cast<uint32_t>(fbh)};
    spec.format = configure_surface(surface, gpu.adapter, gpu.device, spec.width, spec.height);
    render::QuadRenderer quads;
    if (!init_quads(quads, gpu, spec.format, level, fighter)) {
        std::fprintf(stderr, "neon-rumble: quad renderer or level texture upload failed\n");
        return 1;
    }
    Layers layers;
    FighterQuads fighter_quads;
    const WGPUColor clear = background(level, spec.format);
    bool cropped = false;
    bool overlay = false;
    bool f3_held = false;
    bool warned = false;
    bool upload_warned = false;
    bool fighter_warned = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (overlay_toggled(window, f3_held)) overlay = !overlay;
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
        if (!sf.texture) continue;
        scene.step(static_cast<uint32_t>(drawn));
        const auto tick = static_cast<uint64_t>(drawn);
        const ViewportFit fit = framework::graphics::viewport_fit({spec.width, spec.height});
        if (fit.cropped && !cropped)
            std::fprintf(stderr, "neon-rumble: window %ux%u is smaller than the %ux%u zone, the zone is cropped\n",
                         spec.width, spec.height, framework::graphics::VIEW_ZONE.w, framework::graphics::VIEW_ZONE.h);
        cropped = fit.cropped;
        LayerStats st = layers.build(level, fit, tick);
        const FighterStats fs = fighter_quads.add(fighter, fighter.pose(tick), layers, st, level.texture_count, overlay);
        if (fs.rejected + fs.dropped > 0 && !fighter_warned) {
            std::fprintf(stderr, "neon-rumble: tick %llu: fighter %u quad(s) rejected, %u dropped\n",
                         static_cast<unsigned long long>(tick), fs.rejected, fs.dropped);
            fighter_warned = true;
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

int run_window(Scene& scene, const Level& level, const Fighter& fighter, int frames) {
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
        rc = frame_loop(window, gpu, surface, scene, level, fighter, frames, drawn);
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
