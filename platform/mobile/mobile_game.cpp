#include "mobile_game.hpp"

#include "codes.hpp"
#include "draw.hpp"
#include "sim.hpp"
#include "stick.hpp"

namespace game {
namespace {

struct Btn { float cx, cy, r; };

// Кнопка-огонь: правый-нижний угол, радиус/отступ как доля короткой стороны →
// вид (тач) и мир (рендер) пропорциональны одному экрану → совпадают визуально.
Btn fire_btn(float w, float h) {
    const float s = w < h ? w : h;
    const float r = 0.13f * s, m = 0.045f * s;
    return {w - m - r, h - m - r, r};
}

} // namespace

bool MobileGame::init(GpuContext& gpu, WGPUSurface surface, uint32_t fb_w, uint32_t fb_h,
                      const std::string& audio_bundle) {
    if (inited_) shutdown();   // симметрично: повторный init освобождает прошлые ресурсы
    gpu_ = &gpu;
    world_ = flecs::world();   // re-entrant: свежий мир (Android может пере-init после teardown)
    gs_ = GameState{};
    fx_.clear();
    tick_ = 0;
    stick_id_ = fire_id_ = -1;
    fire_down_ = false;

    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface, gpu.adapter, &caps);
    fmt_ = caps.formatCount ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = gpu.device; cfg.format = fmt_;
    cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    cfg.width = fb_w; cfg.height = fb_h; cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    use_bloom_ = bloom_.init(gpu.device, gpu.queue, fmt_, fb_w, fb_h);
    atlas_ = build_atlas();
    batch_.init(gpu.device, gpu.queue,
                use_bloom_ ? WGPUTextureFormat_RGBA16Float : fmt_, atlas_);

    const double sa = (double)fb_w / fb_h, wa = (double)VIEW_W / VIEW_H;
    const uint32_t world_w = sa >= wa ? (uint32_t)(VIEW_H * sa) : VIEW_W;
    const uint32_t world_h = sa >= wa ? VIEW_H : (uint32_t)(VIEW_W / sa);
    batch_.set_viewport(world_w, world_h);
    vw_ = (float)world_w; vh_ = (float)world_h;

    spawn(world_, gs_);
    map_ = make_map();
    engine_ = new input::InputEngine(map_);
    engine_->post({input::RawKind::DeviceConnected, input::DeviceKind::Gamepad, 0, 0, 0, seq_++});
    audio_.init(audio_bundle);   // graceful: нет устройства/бандла → false → no-op
    inited_ = true;
    return true;
}

void MobileGame::post_axis(fix32 x, fix32 y) {
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   (uint16_t)input::code::LX, x.raw, seq_++});
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   (uint16_t)input::code::LY, y.raw, seq_++});
}

void MobileGame::post_fire(bool down) {
    fire_down_ = down;
    engine_->post({down ? input::RawKind::PadButtonDown : input::RawKind::PadButtonUp,
                   input::DeviceKind::Gamepad, 0, (uint16_t)input::code::PadA, 0, seq_++});
}

void MobileGame::pointer(int id, Touch phase, float px, float py, float view_w, float view_h) {
    if (!engine_) return;
    const Btn b = fire_btn(view_w, view_h);
    const float stick_r = 0.12f * (view_w < view_h ? view_w : view_h);
    if (phase == Touch::Down) {
        const float dx = px - b.cx, dy = py - b.cy;
        if (dx * dx + dy * dy <= b.r * b.r) {            // в круге кнопки → только огонь, не стик
            if (fire_id_ == -1) { fire_id_ = id; post_fire(true); }
        } else if (stick_id_ == -1) { stick_id_ = id; stick_ox_ = px; stick_oy_ = py; }
    } else if (phase == Touch::Move) {
        if (id == stick_id_) {
            const StickAxis a = stick_axis(px - stick_ox_, py - stick_oy_, stick_r);
            post_axis(a.x, a.y);
        }
    } else {
        if (id == fire_id_) { fire_id_ = -1; post_fire(false); }
        if (id == stick_id_) { stick_id_ = -1; post_axis(fix32{}, fix32{}); }
    }
}

void MobileGame::cancel() {
    if (!engine_) return;
    if (fire_id_ != -1) { fire_id_ = -1; post_fire(false); }
    if (stick_id_ != -1) { stick_id_ = -1; post_axis(fix32{}, fix32{}); }
}

void MobileGame::demo_drive() {
    static const int DX[8] = {1, 0, -1, 0, 1, -1, -1, 1};
    static const int DY[8] = {0, -1, 0, 1, -1, -1, 1, 1};
    const int seg = (tick_ / 45) % 8;
    post_axis(fix32::from_int(DX[seg]), fix32::from_int(DY[seg]));
    if (tick_ == 60) post_fire(true);   // press → intro→play; удержан → непрерывный огонь → босс гибнет
}

void MobileGame::push_fire_button() {
    const Btn b = fire_btn(vw_, vh_);
    const float cx = b.cx - vw_ * 0.5f;    // вид (origin top-left, y вниз) → мир (центр, y вверх)
    const float cy = vh_ * 0.5f - b.cy;
    const float d = b.r * 1.8f, k = fire_down_ ? 1.9f : 0.7f;
    batch_.push({cx, cy, d, d, atlas_.star.u0, atlas_.star.v0, atlas_.star.u1, atlas_.star.v1,
                 0.5f * k, 0.9f * k, 1.1f * k, fire_down_ ? 0.85f : 0.5f, 0});
}

void MobileGame::frame(WGPUSurface surface) {
    if (demo_ && stick_id_ == -1 && fire_id_ == -1) demo_drive();
    const fix32 dt = fix32::from_float(1.0 / 60);
    const input::InputFrame& f = engine_->begin_tick(tick_++, 0);
    sink_.events.clear();
    step(world_, gs_, f, dt, &sink_);
    fx_.emit(sink_);
    if (gs_.phase == PH_Play || gs_.phase == PH_Boss) fx_.emit_trails(world_);
    fx_.update(1.0f / 60);
    audio_.on_events(sink_);

    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        if (st.texture) wgpuTextureRelease(st.texture);
        return;
    }
    WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
    batch_.begin();
    push_scene(batch_, world_, atlas_);
    fx_.render(batch_, atlas_);
    push_hud(batch_, world_, atlas_, gs_);
    push_screen(batch_, atlas_, gs_);
    push_fire_button();
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu_->device, nullptr);
    WGPURenderPassEncoder pass = begin_clear(enc, use_bloom_ ? bloom_.hdr_view() : view);
    batch_.flush(pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    if (use_bloom_) bloom_.resolve(enc, view);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu_->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuSurfacePresent(surface);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);
}

void MobileGame::shutdown() {
    if (!inited_) return;   // идемпотентно: без парного init нечего освобождать (нет double-free)
    inited_ = false;
    audio_.shutdown();
    delete engine_;
    engine_ = nullptr;
    bloom_.shutdown();
    batch_.shutdown();
}

} // namespace game
