#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <chrono>
#include <cstdio>
#include <thread>

#include "achievements.hpp"
#include "app.hpp"
#include "fx.hpp"
#include "bloom.hpp"
#include "audio.hpp"
#include "assets_path.hpp"
#include "art.hpp"
#include "batch.hpp"
#include "draw.hpp"
#include "gpu.hpp"
#include "gpu_env.hpp"
#include "material_fx.hpp"
#include "platform_env.hpp"
#include "sim.hpp"
#include "source.hpp"
#include "input_setup.hpp"
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
    GLFWwindow* win = glfwCreateWindow(VIEW_W, VIEW_H, "like-nes - sidescroller (S2a)", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    GpuContext gpu;
    apply_gpu_env(gpu);
    gpu.instance = gpu.create_instance();
    WGPUSurface surface = glfwGetWGPUSurface(gpu.instance, win);
    if (!gpu.init(surface)) { gpu.shutdown(); glfwDestroyWindow(win); glfwTerminate(); return 1; }
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    WGPUTextureFormat fmt = configure_surface(surface, gpu.adapter, gpu.device,
                                              (uint32_t)fbw, (uint32_t)fbh);

    Atlas atlas = load_game_atlas(gpu.supports_bc);
    // Библиотека эффектов (гейт 9 спеки #18). Отказ не фатален: `bind` вернёт false, и сцена
    // нарисуется базовым пайплайном — играть можно без эффектов, но не без окна.
    MaterialFx materials;
    SceneFx sfx;
    const bool have_fx = materials.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA16Float,
                                        resolve_asset("library.bundle").c_str()) && sfx.bind(&materials);
    std::printf("[game] materials: %s (%u pipeline(s), %u fallback(s))\n", have_fx ? "on" : "off",
                materials.pipelines_created(), materials.fallbacks());
    SpriteBatch batch;
    batch.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA16Float, atlas,
               have_fx ? &materials : nullptr);   // → HDR (bloom)
    Bloom bloom;
    if (!bloom.init(gpu.device, gpu.queue, fmt, (uint32_t)fbw, (uint32_t)fbh)) {
        std::fprintf(stderr, "bloom init failed\n");
        gpu.shutdown(); glfwDestroyWindow(win); glfwTerminate(); return 1;
    }

    flecs::world world;
    GameState gs;
    spawn(world, gs);
    Fx fx;
    const TrailQuery trails = make_trail_query(world);
    FxSink sink;
    GameAudio audio;
    const bool have_audio = audio.init(resolve_asset("audio.bundle"));
    std::printf("[game] audio: %s\n", have_audio ? "on (SFX + music)" : "off");
    Achievements ach;
    std::string ach_plugin;
    platform::env_var("LIKENES_ACH_PLUGIN", ach_plugin);
    ach.init(resolve_bundle_path(), resolve_save_path("achievements.save"), ach_plugin);
    std::printf("[game] achievements: %zu defined, %zu unlocked, backend: %s\n",
                ach.defined_count(), ach.unlocked_count(), ach.has_backend() ? "plugin" : "local");

    Controls controls;
    if (!load_controls(controls)) { std::fprintf(stderr, "controls unavailable\n"); return 1; }
    input::InputEngine engine(controls.map);
    install_glfw_input(win, engine);
    input::GamepadSource* pad = input::make_gamepad_source();
    bool have_pad = pad && pad->init();
    std::printf("[game] WASD/arrows/LStick = move | gamepad: %s | Esc = quit\n",
                have_pad ? pad->backend_name() : "none");

    const fix32 dt = fix32::from_float(1.0 / 60);
    const auto period = std::chrono::microseconds(16667);
    auto next = std::chrono::steady_clock::now();
    int frames = 0;
    bool surface_warned = false;
    for (uint32_t t = 0; !glfwWindowShouldClose(win); ++t) {
        next += period;
        std::this_thread::sleep_until(next);
        glfwPollEvents();
        if (have_pad) pad->poll(engine);
        const input::InputFrame& f = engine.begin_tick(t, 0);
        sink.events.clear();
        step(world, gs, f, dt, &sink);
        fx.emit(sink);
        if (gs.phase == PH_Play || gs.phase == PH_Boss) fx.emit_trails(trails);
        fx.update();
        audio.on_events(sink);
        ach.observe(gs);                                 // наблюдатель: sim о нём не знает
        if ((t % 60) == 0) ach.pump();                   // доставка — вне тика
        ach.autosave();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        WGPUSurfaceTexture st = {};
        wgpuSurfaceGetCurrentTexture(surface, &st);
        if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            if (st.texture) wgpuTextureRelease(st.texture);
            // Молчаливый continue означал бесконечный чёрный кадр: Outdated поверхность сама не
            // чинится, её надо переконфигурировать, а не ждать. Размер берём тот же — окно
            // нерастяжимое, и bloom-таргеты созданы под него.
            if (st.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
                st.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
                configure_surface(surface, gpu.adapter, gpu.device, (uint32_t)fbw, (uint32_t)fbh);
            }
            if (!surface_warned) {
                std::fprintf(stderr, "[game] surface texture status %u - frame skipped\n",
                             (unsigned)st.status);
                surface_warned = true;
            }
            continue;
        }
        WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
        batch.begin();
        push_scene(batch, world, atlas, sfx);
        push_fx(batch, fx, atlas);
        push_hud(batch, world, atlas, gs);
        push_screen(batch, atlas, gs);
        push_toast(batch, atlas, ach.toast().name.c_str(), ach.toast().left);
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
        WGPURenderPassEncoder pass = begin_clear(enc, bloom.hdr_view(), WGPUColor{0.02, 0.02, 0.07, 1.0});   // сцена → HDR
        batch.flush(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        bloom.resolve(enc, view);                                          // bloom → swapchain
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
        wgpuQueueSubmit(gpu.queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);
        wgpuSurfacePresent(surface);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(st.texture);
        if (frame_cap && ++frames >= frame_cap) break;
    }
    ach.pump();
    ach.save();
    audio.shutdown();
    bloom.shutdown();
    batch.shutdown();
    materials.shutdown();
    wgpuSurfaceRelease(surface);
    gpu.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    std::printf("[game] fx: peak %u of %u, dropped %u\n", fx.peak(), FX_CAP, fx.dropped());
    std::printf("[game] window clean exit\n");
    return 0;
}

} // namespace game
