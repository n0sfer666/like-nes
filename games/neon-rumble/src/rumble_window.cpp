#include "rumble.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cstdio>
#include <span>

#include "gpu.hpp"
#include "quad_batch.hpp"
#include "rumble_layers.hpp"
#include "surface_frame.hpp"

namespace rumble {

namespace {

void draw_frame(const GpuContext& gpu, WGPUSurface surface, WGPUTextureView view,
                const render::QuadRenderer& quads, std::span<const render::QuadRun> runs) {
    WGPURenderPassColorAttachment color = {};
    color.view = view;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = WGPUColor{0.06, 0.02, 0.12, 1.0};

    WGPURenderPassDescriptor rp = {};
    rp.label = "neon-rumble-layers";
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &color;

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
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
// материала спрайта, который `draw_layer` выдаёт по guid.
bool init_quads(render::QuadRenderer& quads, const GpuContext& gpu, WGPUTextureFormat format,
                const Level& level) {
    if (!quads.init(gpu.device, gpu.queue, format, Layers::CAPACITY)) return false;
    for (uint32_t i = 0; i < level.texture_count; ++i)
        if (!quads.add_texture(level.pixels[i].pixels, level.pixels[i].width,
                               level.pixels[i].height))
            return false;
    return true;
}

int frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Scene& scene,
               const Level& level, int frames, int& drawn) {
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    SurfaceSpec spec{surface, WGPUTextureFormat_Undefined, static_cast<uint32_t>(fbw),
                     static_cast<uint32_t>(fbh)};
    spec.format = configure_surface(surface, gpu.adapter, gpu.device, spec.width, spec.height);
    render::QuadRenderer quads;
    if (!init_quads(quads, gpu, spec.format, level)) {
        std::fprintf(stderr, "neon-rumble: quad renderer or level texture upload failed\n");
        return 1;
    }
    Layers layers;
    bool warned = false;
    bool upload_warned = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
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
        const LayerStats st = layers.build(level, spec.width, spec.height,
                                           static_cast<uint64_t>(drawn));
        if (!quads.upload(layers.quads(st), spec.width, spec.height) && !upload_warned) {
            std::fprintf(stderr, "neon-rumble: %u quad(s) not uploaded, frame left empty\n", st.quads);
            upload_warned = true;
        }
        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        draw_frame(gpu, surface, view, quads, layers.runs(st));
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(sf.texture);
        if (++drawn == frames) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    return 0;
}

} // namespace

int run_window(Scene& scene, const Level& level, int frames) {
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
        rc = frame_loop(window, gpu, surface, scene, level, frames, drawn);
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
