#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include "fixed.hpp"
#include "surface_frame.hpp"

#include <cstdio>
#include <cstdint>

static void render_frame(WGPUDevice device, WGPUQueue queue, WGPUSurface surface,
                         WGPUTextureView view, int frame) {
    // Анимированный clear-color: фаза считается в fix32 (симуляционный домен),
    // в double конвертируется ТОЛЬКО здесь, на границе рендера (инвариант «GPU не кормит сим»).
    fix32 phase = fix32::from_int(frame) * fix32::from_float(0.02);
    double t = phase.to_double();
    double pulse = t - static_cast<double>(static_cast<long>(t)); // пила 0..1

    WGPURenderPassColorAttachment color = {};
    color.nextInChain = nullptr;
    color.view = view;
    color.resolveTarget = nullptr;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = WGPUColor{0.1 * pulse, 0.2, 0.35 + 0.3 * pulse, 1.0};

    WGPURenderPassDescriptor rp = {};
    rp.nextInChain = nullptr;
    rp.label = "clear-pass";
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &color;
    rp.depthStencilAttachment = nullptr;
    rp.occlusionQuerySet = nullptr;
    rp.timestampWrites = nullptr;

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuSurfacePresent(surface);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

int main() {
    int rc = 0;
    int frames = 0, ticks = 0;
    bool warned = false;
    SurfaceSpec spec;
    GpuContext gpu;
    GLFWwindow* window = nullptr;
    WGPUSurface surface = nullptr;

    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(960, 540, "like-nes PoC", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "window failed\n"); rc = 1; goto cleanup; }

    gpu.instance = gpu.create_instance();
    if (!gpu.instance) { std::fprintf(stderr, "instance failed\n"); rc = 1; goto cleanup; }
    surface = glfwGetWGPUSurface(gpu.instance, window);
    if (!gpu.init(surface)) { std::fprintf(stderr, "no adapter or device\n"); rc = 1; goto cleanup; }

    {
        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        spec = SurfaceSpec{surface, WGPUTextureFormat_Undefined, static_cast<uint32_t>(fbw),
                           static_cast<uint32_t>(fbh)};
        spec.format = configure_surface(surface, gpu.adapter, gpu.device, spec.width, spec.height);
        std::printf("[core-smoke] webgpu up: fb=%dx%d format=%d\n", fbw, fbh,
                    static_cast<int>(spec.format));
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (++ticks >= 120) glfwSetWindowShouldClose(window, GLFW_TRUE);
        int nw = 0, nh = 0;
        glfwGetFramebufferSize(window, &nw, &nh);
        if (nw > 0 && nh > 0 && (static_cast<uint32_t>(nw) != spec.width
                                 || static_cast<uint32_t>(nh) != spec.height)) {
            spec.width = static_cast<uint32_t>(nw);
            spec.height = static_cast<uint32_t>(nh);
            reconfigure_surface(spec, gpu.device);
        }
        const SurfaceFrame sf = acquire_frame(spec, gpu, "core-smoke", warned);
        if (sf.quit) { rc = 1; break; }
        if (!sf.texture) continue;

        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        render_frame(gpu.device, gpu.queue, surface, view, frames);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(sf.texture);
        ++frames;
    }
    if (rc == 0 && frames == 0) {
        std::fprintf(stderr, "[core-smoke] window exit after %d frames, none drawn\n", ticks);
        rc = 1;
    }

cleanup:
    if (surface) wgpuSurfaceRelease(surface);
    gpu.shutdown();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
    if (rc == 0) std::printf("[core-smoke] clean exit after %d rendered frames\n", frames);
    return rc;
}
