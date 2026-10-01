#include "rumble.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <cstdio>

#include "gpu.hpp"
#include "surface_frame.hpp"

namespace rumble {

namespace {

void clear_frame(const GpuContext& gpu, WGPUSurface surface, WGPUTextureView view) {
    WGPURenderPassColorAttachment color = {};
    color.view = view;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = WGPUColor{0.06, 0.02, 0.12, 1.0};

    WGPURenderPassDescriptor rp = {};
    rp.label = "neon-rumble-clear";
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &color;

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuSurfacePresent(surface);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

int frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Scene& scene,
               int frames, int& drawn) {
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    SurfaceSpec spec{surface, WGPUTextureFormat_Undefined, static_cast<uint32_t>(fbw),
                     static_cast<uint32_t>(fbh)};
    spec.format = configure_surface(surface, gpu.adapter, gpu.device, spec.width, spec.height);
    bool warned = false;
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
        WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
        clear_frame(gpu, surface, view);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(sf.texture);
        if (++drawn == frames) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    return 0;
}

} // namespace

int run_window(Scene& scene, int frames) {
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
        rc = frame_loop(window, gpu, surface, scene, frames, drawn);
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
