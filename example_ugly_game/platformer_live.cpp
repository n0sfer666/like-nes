#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "art.hpp"
#include "assets_path.hpp"
#include "batch.hpp"
#include "gpu.hpp"
#include "gpu_env.hpp"
#include "input_setup.hpp"
#include "platform_args.hpp"
#include "platformer_input.hpp"
#include "platformer_view.hpp"
#include "source.hpp"

// Живая половина образца-платформера (гейт 8 спеки #16): окно, часы и ввод. Всё остальное —
// чужое и общее с гейтами: шаг мира тот же `step_stage`, что гоняет sim-голден, а камера и квады
// те же, что проверяет `game_platformer_view_test`. Разойтись живому прогону и гейту здесь нечем —
// у них буквально один код, и этот файл только подаёт ему ввод и показывает результат.
//
// Тонким он остаётся нарочно: у окна нет ни одного утверждения, которое можно проверить на раннере,
// поэтому каждая строка, которую МОЖНО унести в чистую половину, туда унесена.
namespace platformer {
namespace {

constexpr int WIN_W = VIEW_W * VIEW_SCALE;
constexpr int WIN_H = VIEW_H * VIEW_SCALE;

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

// Квад вида → инстанс батча. Батч меряет мир в ЮНИТАХ ВИДА (`set_viewport(VIEW_W, VIEW_H)`), а не в
// пикселях окна: скейл тогда целиком в размере окна, и растяжение делает растеризатор — ровно один
// раз и одинаково для всех квадов. Оси у батча свои: центр квада и +Y ВВЕРХ.
// Распаковка RGBA8 в доли единицы. Тон приезжает из чистой половины упакованным (вертикаль 3 спеки
// #17): столько его и несёт спрайт фреймворка, и разрядов, которых в нём нет, здесь не появится.
float channel(uint32_t rgba, unsigned shift) {
    return static_cast<float>((rgba >> shift) & 0xffu) / 255.0f;
}

void push_frame(game::SpriteBatch& batch, const game::Atlas& atlas, const std::vector<Quad>& qs) {
    for (const Quad& q : qs) {
        game::Instance i;
        i.x = q.x + q.w * 0.5f - VIEW_W * 0.5f;
        i.y = VIEW_H * 0.5f - (q.y + q.h * 0.5f);
        i.w = q.w;
        i.h = q.h;
        i.u0 = atlas.solid.u0; i.v0 = atlas.solid.v0;
        i.u1 = atlas.solid.u1; i.v1 = atlas.solid.v1;
        i.r = channel(q.color, 24); i.g = channel(q.color, 16);
        i.b = channel(q.color, 8); i.a = channel(q.color, 0);
        batch.push(i);
    }
}

constexpr WGPUColor SKY{0.05, 0.06, 0.10, 1.0};

int run(const std::string& bundle) {
    Stage stage;
    if (!load_stage(bundle, stage)) {
        std::fprintf(stderr, "[platformer] level unreadable: %s\n", bundle.c_str());
        return 1;
    }
    game::Controls controls;
    Binding bind;
    if (!game::load_controls(controls) || !resolve_binding(controls.table, "default", bind)) {
        std::fprintf(stderr, "[platformer] controls unavailable: need actions 'jump' and axes "
                             "'move_x'/'move_y' in the bundle preset\n");
        return 1;
    }

    if (!glfwInit()) { std::fprintf(stderr, "[platformer] glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(WIN_W, WIN_H, "like-nes - platformer", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    GpuContext gpu;
    game::apply_gpu_env(gpu);
    gpu.instance = gpu.create_instance();
    WGPUSurface surface = glfwGetWGPUSurface(gpu.instance, win);
    if (!gpu.init(surface)) { gpu.shutdown(); glfwDestroyWindow(win); glfwTerminate(); return 1; }
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    const WGPUTextureFormat fmt = configure_surface(surface, gpu.adapter, gpu.device,
                                                    static_cast<uint32_t>(fbw),
                                                    static_cast<uint32_t>(fbh));

    // Атлас ПРОЦЕДУРНЫЙ, а не бейкнутый: рисуем плоские квады через tint, то есть из всего атласа
    // нужен один белый блок. Бейкнутый притащил бы за собой транскод UASTC->BC7 ради этого блока.
    game::Atlas atlas = game::build_atlas();
    game::SpriteBatch batch;
    batch.init(gpu.device, gpu.queue, fmt, atlas);
    batch.set_viewport(VIEW_W, VIEW_H);

    ::input::InputEngine engine(controls.map);
    ::input::install_glfw_input(win, engine);
    ::input::GamepadSource* pad = ::input::make_gamepad_source();
    const bool have_pad = pad && pad->init();
    std::printf("[platformer] WASD/arrows = move | space/up = jump | down+jump = drop | "
                "gamepad: %s | Esc = quit\n", have_pad ? pad->backend_name() : "none");

    std::vector<Quad> quads;
    FrameSprites sprites;
    const auto period = std::chrono::microseconds(16667);
    auto next = std::chrono::steady_clock::now();
    bool surface_warned = false;
    for (uint32_t t = 0; !glfwWindowShouldClose(win); ++t) {
        next += period;
        std::this_thread::sleep_until(next);
        glfwPollEvents();
        if (have_pad) pad->poll(engine);
        step_stage(stage, read_input(engine.begin_tick(t, 0), bind));
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        WGPUSurfaceTexture st = {};
        wgpuSurfaceGetCurrentTexture(surface, &st);
        if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            if (st.texture) wgpuTextureRelease(st.texture);
            // Молчаливый `continue` означал бы вечный чёрный кадр: устаревшая поверхность сама не
            // чинится, её надо переконфигурировать. Тот же случай и то же лечение, что в `live.cpp`.
            if (st.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
                st.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
                configure_surface(surface, gpu.adapter, gpu.device, static_cast<uint32_t>(fbw),
                                  static_cast<uint32_t>(fbh));
            }
            if (!surface_warned) {
                std::fprintf(stderr, "[platformer] surface texture status %u - frame skipped\n",
                             static_cast<unsigned>(st.status));
                surface_warned = true;
            }
            continue;
        }
        WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
        build_quads(stage, camera_at(stage), sprites, quads);
        batch.begin();
        push_frame(batch, atlas, quads);
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
        WGPURenderPassEncoder pass = game::begin_clear(enc, view, SKY);
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
    }
    batch.shutdown();
    wgpuSurfaceRelease(surface);
    gpu.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    std::printf("[platformer] window clean exit\n");
    return 0;
}

} // namespace
} // namespace platformer

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string bundle = game::resolve_bundle_path();
    for (int i = 1; i < argc; ++i) bundle = argv[i];
    if (bundle.empty()) {
        std::fprintf(stderr, "[platformer] game.bundle not found next to the executable\n");
        return 1;
    }
    return platformer::run(bundle);
}
