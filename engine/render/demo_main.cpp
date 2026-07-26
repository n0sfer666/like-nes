#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include "capture.hpp"
#include "gpu.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "sprite.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t DUMP_W = 960, DUMP_H = 540, DUMP_FRAME = 90;

WGPUTextureFormat configure_surface(WGPUSurface surface, WGPUAdapter adapter,
                                    WGPUDevice device, uint32_t w, uint32_t h) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface, adapter, &caps);
    WGPUTextureFormat format = caps.formatCount > 0 ? caps.formats[0]
                                                     : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = device; cfg.format = format;
    cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    cfg.width = w; cfg.height = h; cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return format;
}

int run_dump(const char* path) {
    GpuContext gpu;
    if (!gpu.init(nullptr)) { gpu.shutdown(); return 1; }
    Sprite sprite; sprite.init(gpu.device, gpu.queue);
    Renderer renderer;
    renderer.init(gpu.device, gpu.queue, sprite, WGPUTextureFormat_RGBA8Unorm, DUMP_W, DUMP_H);

    Scene scene;
    for (uint32_t i = 0; i < DUMP_FRAME; ++i) scene.advance();
    SceneSnapshot snap = scene.snapshot((float)DUMP_W / DUMP_H);

    std::vector<uint8_t> px = capture::render_offscreen(gpu.device, gpu.queue, renderer,
                                                        snap, DUMP_W, DUMP_H);
    const bool ok = !px.empty() && capture::write_png(path, px, DUMP_W, DUMP_H);
    std::printf("[render] dump %s: %s (arena pool=%zu allocs=%llu)\n", path,
                ok ? "OK" : "FAIL", renderer.arena().pool_size(),
                (unsigned long long)renderer.arena().allocations());

    renderer.shutdown(); sprite.shutdown(); gpu.shutdown();
    return ok ? 0 : 1;
}

int run_window() {
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(960, 540, "like-nes render PoC", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    GpuContext gpu;
    gpu.instance = wgpuCreateInstance(nullptr);
    WGPUSurface surface = glfwGetWGPUSurface(gpu.instance, window);
    if (!gpu.init(surface)) {
        wgpuSurfaceRelease(surface); gpu.shutdown();
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    WGPUTextureFormat fmt = configure_surface(surface, gpu.adapter, gpu.device,
                                              (uint32_t)fbw, (uint32_t)fbh);

    Sprite sprite; sprite.init(gpu.device, gpu.queue);
    Renderer renderer;
    renderer.init(gpu.device, gpu.queue, sprite, fmt, (uint32_t)fbw, (uint32_t)fbh);
    Scene scene;

    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        WGPUSurfaceTexture st = {};
        wgpuSurfaceGetCurrentTexture(surface, &st);
        if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            if (st.texture) wgpuTextureRelease(st.texture);
            continue;
        }
        WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
        scene.advance();
        renderer.render(scene.snapshot((float)fbw / fbh), view);
        wgpuSurfacePresent(surface);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(st.texture);
        if (++frames >= 600) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    renderer.shutdown(); sprite.shutdown();
    wgpuSurfaceRelease(surface); gpu.shutdown();
    glfwDestroyWindow(window); glfwTerminate();
    std::printf("[render] window clean exit after %d frames\n", frames);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--dump") == 0) return run_dump(argv[i + 1]);
    return run_window();
}
