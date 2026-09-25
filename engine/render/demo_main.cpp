#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include "capture.hpp"
#include "gpu.hpp"
#include "platform_args.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "sprite.hpp"
#include "surface_frame.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t DUMP_W = 960, DUMP_H = 540, DUMP_FRAME = 90;

int run_dump(const char* path) {
    GpuContext gpu;
    if (!gpu.init(nullptr)) { gpu.shutdown(); return 1; }
    Sprite sprite; sprite.init(gpu.device, gpu.queue);
    Renderer renderer;
    if (!renderer.init(gpu.device, gpu.queue, sprite, WGPUTextureFormat_RGBA8Unorm, DUMP_W, DUMP_H)) {
        renderer.shutdown(); sprite.shutdown(); gpu.shutdown();
        return 1;
    }

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
    if (!renderer.init(gpu.device, gpu.queue, sprite, fmt, (uint32_t)fbw, (uint32_t)fbh)) {
        renderer.shutdown(); sprite.shutdown();
        wgpuSurfaceRelease(surface); gpu.shutdown();
        glfwDestroyWindow(window); glfwTerminate();
        return 1;
    }
    Scene scene;

    const SurfaceSpec spec{surface, fmt, static_cast<uint32_t>(fbw), static_cast<uint32_t>(fbh)};
    int frames = 0, drawn = 0;
    bool warned = false, lost = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const SurfaceFrame f = acquire_frame(spec, gpu, "render", warned);
        if (f.quit) { lost = true; break; }
        if (f.texture) {
            ++drawn;
            WGPUTextureView view = wgpuTextureCreateView(f.texture, nullptr);
            scene.advance();
            renderer.render(scene.snapshot((float)fbw / fbh), view);
            wgpuSurfacePresent(surface);
            wgpuTextureViewRelease(view);
            wgpuTextureRelease(f.texture);
        }
        if (++frames >= 600) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    renderer.shutdown(); sprite.shutdown();
    wgpuSurfaceRelease(surface); gpu.shutdown();
    glfwDestroyWindow(window); glfwTerminate();
    if (lost) return 1;
    // Такта, кроме `Fifo`, у петли нет: залипшая поверхность прокручивает 600 пропусков за
    // миллисекунды, и без счёта нарисованных это был бы «clean exit» без единого кадра.
    if (drawn == 0) {
        std::fprintf(stderr, "[render] window exit after %d frames, none drawn\n", frames);
        return 1;
    }
    std::printf("[render] window clean exit after %d frames, %d drawn\n", frames, drawn);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--dump") == 0) return run_dump(argv[i + 1]);
    return run_window();
}
