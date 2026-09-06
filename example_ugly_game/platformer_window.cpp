#include "platformer_window.hpp"

#include <glfw3webgpu.h>

#include <cstdio>

#include "gpu_env.hpp"

namespace platformer {
namespace {

constexpr int WIN_W = VIEW_W * VIEW_SCALE;
constexpr int WIN_H = VIEW_H * VIEW_SCALE;

constexpr WGPUColor SKY{0.05, 0.06, 0.10, 1.0};

WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d,
                                    uint32_t w, uint32_t h) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(s, a, &caps);
    WGPUTextureFormat fmt = caps.formatCount ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = d; cfg.format = fmt; cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto; cfg.width = w; cfg.height = h;
    // `Fifo` — не вкус, а ЧАСЫ образца: ровно им живой прогон и сетевой пир держат 60 Гц, и без
    // него сессия шла бы со скоростью видеокарты.
    cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(s, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return fmt;
}

// Распаковка RGBA8 в доли единицы. Тон приезжает из чистой половины упакованным (вертикаль 3 спеки
// #17): столько его и несёт спрайт фреймворка, и разрядов, которых в нём нет, здесь не появится.
float channel(uint32_t rgba, unsigned shift) {
    return static_cast<float>((rgba >> shift) & 0xffu) / 255.0f;
}

// Квад вида → инстанс батча. Батч меряет мир в ЮНИТАХ ВИДА (`set_viewport(VIEW_W, VIEW_H)`), а не в
// пикселях окна: скейл тогда целиком в размере окна, и растяжение делает растеризатор — ровно один
// раз и одинаково для всех квадов. Оси у батча свои: центр квада и +Y ВВЕРХ.
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

} // namespace

bool Window::open(const char* title) {
    if (!glfwInit()) { std::fprintf(stderr, "[platformer] glfwInit failed\n"); return false; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    win_ = glfwCreateWindow(WIN_W, WIN_H, title, nullptr, nullptr);
    if (!win_) { glfwTerminate(); return false; }

    game::apply_gpu_env(gpu_);
    gpu_.instance = gpu_.create_instance();
    // Обе половины спрашиваются ПОИМЁННО. Инстанса нет — до `glfwGetWGPUSurface` дело не доходит
    // вовсе; поверхности нет — `gpu_.init` увидел бы null и отказал бы уже про адаптер. Оба случая
    // приезжают к владельцу кодом 1, и §14 велит ему назвать GPU и драйвер: строка причины и есть
    // всё, что он может приложить.
    if (gpu_.instance == nullptr) {
        std::fprintf(stderr, "[platformer] no wgpu instance\n");
        close();
        return false;
    }
    surface_ = glfwGetWGPUSurface(gpu_.instance, win_);
    if (surface_ == nullptr) {
        std::fprintf(stderr, "[platformer] the window gave no wgpu surface\n");
        close();
        return false;
    }
    if (!gpu_.init(surface_)) { close(); return false; }
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win_, &fbw, &fbh);
    fbw_ = static_cast<uint32_t>(fbw);
    fbh_ = static_cast<uint32_t>(fbh);
    fmt_ = configure_surface(surface_, gpu_.adapter, gpu_.device, fbw_, fbh_);

    // Атлас ПРОЦЕДУРНЫЙ, а не бейкнутый: рисуем плоские квады через tint, то есть из всего атласа
    // нужен один белый блок. Бейкнутый притащил бы за собой транскод UASTC->BC7 ради этого блока.
    atlas_ = game::build_atlas();
    batch_.init(gpu_.device, gpu_.queue, fmt_, atlas_);
    batch_.set_viewport(VIEW_W, VIEW_H);
    return true;
}

void Window::close() {
    if (win_ == nullptr) return;
    batch_.shutdown();
    if (surface_) wgpuSurfaceRelease(surface_);
    surface_ = nullptr;
    gpu_.shutdown();
    glfwDestroyWindow(win_);
    win_ = nullptr;
    glfwTerminate();
}

void Window::poll() { glfwPollEvents(); }

bool Window::quit_asked() const {
    return glfwWindowShouldClose(win_) || glfwGetKey(win_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
}

void Window::draw(const Stage& stage) {
    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(surface_, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        if (st.texture) wgpuTextureRelease(st.texture);
        // Молчаливый выход означал бы вечный чёрный кадр: устаревшая поверхность сама не чинится,
        // её надо переконфигурировать. Тот же случай и то же лечение, что в `live.cpp`.
        if (st.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
            st.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
            configure_surface(surface_, gpu_.adapter, gpu_.device, fbw_, fbh_);
        }
        if (!surface_warned_) {
            std::fprintf(stderr, "[platformer] surface texture status %u - frame skipped\n",
                         static_cast<unsigned>(st.status));
            surface_warned_ = true;
        }
        return;
    }
    WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
    build_quads(stage, camera_at(stage), sprites_, quads_);
    batch_.begin();
    push_frame(batch_, atlas_, quads_);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu_.device, nullptr);
    WGPURenderPassEncoder pass = game::begin_clear(enc, view, SKY);
    batch_.flush(pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu_.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuSurfacePresent(surface_);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);
}

} // namespace platformer
