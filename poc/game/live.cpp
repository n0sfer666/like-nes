#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <chrono>
#include <cstdio>
#include <thread>

#include "app.hpp"
#include "art.hpp"
#include "batch.hpp"
#include "draw.hpp"
#include "gpu.hpp"
#include "sim.hpp"
#include "source.hpp"
#include "world.hpp"

namespace game {
namespace {

WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d,
                                    uint32_t w, uint32_t h) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(s, a, &caps);
    WGPUTextureFormat fmt = caps.formatCount ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = d; cfg.format = fmt; cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto; cfg.width = w; cfg.height = h;
    cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(s, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return fmt;
}

} // namespace

int run_window(int frame_cap) {
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(VIEW_W, VIEW_H, "like-nes — sidescroller (S2a)", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    GpuContext gpu;
    gpu.instance = wgpuCreateInstance(nullptr);
    WGPUSurface surface = glfwGetWGPUSurface(gpu.instance, win);
    if (!gpu.init(surface)) { gpu.shutdown(); glfwDestroyWindow(win); glfwTerminate(); return 1; }
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    WGPUTextureFormat fmt = configure_surface(surface, gpu.adapter, gpu.device,
                                              (uint32_t)fbw, (uint32_t)fbh);

    Atlas atlas = load_game_atlas(gpu.supports_bc);
    SpriteBatch batch;
    batch.init(gpu.device, gpu.queue, fmt, atlas);

    flecs::world world;
    GameState gs;
    spawn(world, gs);
    input::ActionMap map = make_map();
    input::InputEngine engine(map);
    install_glfw_input(win, engine);
    input::GamepadSource* pad = input::make_gamepad_source();
    bool have_pad = pad && pad->init();
    std::printf("[game] WASD/arrows/LStick = move · gamepad: %s · Esc = quit\n",
                have_pad ? pad->backend_name() : "none");

    const fix32 dt = fix32::from_float(1.0 / 60);
    const auto period = std::chrono::microseconds(16667);
    auto next = std::chrono::steady_clock::now();
    int frames = 0;
    for (uint32_t t = 0; !glfwWindowShouldClose(win); ++t) {
        next += period;
        std::this_thread::sleep_until(next);
        glfwPollEvents();
        if (have_pad) pad->poll(engine);
        const input::InputFrame& f = engine.begin_tick(t, 0);
        step(world, gs, f, dt);
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        WGPUSurfaceTexture st = {};
        wgpuSurfaceGetCurrentTexture(surface, &st);
        if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            if (st.texture) wgpuTextureRelease(st.texture);
            continue;
        }
        WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
        batch.begin();
        push_scene(batch, world, atlas);
        push_hud(batch, world, atlas, gs);
        push_screen(batch, atlas, gs);
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
        WGPURenderPassEncoder pass = begin_clear(enc, view);
        batch.flush(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
        wgpuQueueSubmit(gpu.queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);
        wgpuSurfacePresent(surface);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(st.texture);
        if (frame_cap && ++frames >= frame_cap) break;
    }
    batch.shutdown();
    wgpuSurfaceRelease(surface);
    gpu.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    std::printf("[game] window clean exit\n");
    return 0;
}

} // namespace game
