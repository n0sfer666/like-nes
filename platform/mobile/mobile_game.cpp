#include "mobile_game.hpp"

#include <cstdio>

#include "assets_path.hpp"
#include "codes.hpp"
#include "draw.hpp"
#include "game_sim.hpp"
#include "mobile_stick.hpp"

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

bool MobileGame::init(GpuContext& gpu, WGPUSurface surface, uint32_t fb_w, uint32_t fb_h) {
    if (inited_) shutdown();   // симметрично: повторный init освобождает прошлые ресурсы
    gpu_ = &gpu;
    trails_ = TrailQuery();    // запрос старого мира гасится ДО мира
    world_ = flecs::world();   // re-entrant: свежий мир (Android может пере-init после teardown)
    gs_ = GameState{};
    fx_.clear();
    tick_ = 0;
    stick_id_ = fire_id_ = -1;
    fire_down_ = false;

    fb_w_ = fb_w; fb_h_ = fb_h;
    surface_warned_ = lost_ = false;
    fmt_ = configure_surface(surface, gpu.adapter, gpu.device, fb_w, fb_h);

    // Сброс целиком: повторный init не наследует от прошлого ни карту, ни накладку перебиндов.
    controls_ = Controls{};
    if (!load_controls(controls_)) {
        std::fprintf(stderr, "[game] controls unavailable\n");
        return false;
    }
    use_bloom_ = bloom_.init(gpu.device, gpu.queue, fmt_, fb_w, fb_h);
    const WGPUTextureFormat scene_fmt = use_bloom_ ? WGPUTextureFormat_RGBA16Float : fmt_;
    atlas_ = load_game_atlas(gpu.supports_bc);
    const bool mat_ok = materials_.init(gpu.device, gpu.queue, scene_fmt,
                                        resolve_asset("library.bundle").c_str());
    // bind зовётся и при отказе: он же отвязывает sfx_ от материалов прошлого init.
    const bool have_fx = sfx_.bind(mat_ok ? &materials_ : nullptr);
    std::printf("[game] materials: %s (%u pipeline(s), %u fallback(s))\n", have_fx ? "on" : "off",
                materials_.pipelines_created(), materials_.fallbacks());
    batch_.init(gpu.device, gpu.queue, scene_fmt, atlas_, have_fx ? &materials_ : nullptr);

    const double sa = static_cast<double>(fb_w) / fb_h, wa = static_cast<double>(VIEW_W) / VIEW_H;
    const uint32_t world_w = sa >= wa ? static_cast<uint32_t>(VIEW_H * sa) : VIEW_W;
    const uint32_t world_h = sa >= wa ? VIEW_H : static_cast<uint32_t>(VIEW_W / sa);
    batch_.set_viewport(world_w, world_h);
    vw_ = static_cast<float>(world_w); vh_ = static_cast<float>(world_h);

    spawn(world_, gs_);
    trails_ = make_trail_query(world_);
    engine_ = new input::InputEngine(controls_.map);
    engine_->post({input::RawKind::DeviceConnected, input::DeviceKind::Gamepad, 0, 0, 0, seq_++});
    const bool have_audio = audio_.init(resolve_asset("audio.bundle"));   // graceful: нет → no-op
    std::printf("[game] audio: %s\n", have_audio ? "on (SFX + music)" : "off");
    // Нативный бэкенд-плагин на мобиле не грузится: достижения живут локально, в песочнице.
    ach_.emplace();
    ach_->init(resolve_bundle_path(), resolve_save_path("achievements.save"), "");
    std::printf("[game] achievements: %zu defined, %zu unlocked\n", ach_->defined_count(),
                ach_->unlocked_count());
    inited_ = true;
    return true;
}

void MobileGame::post_axis(fix32 x, fix32 y) {
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   static_cast<uint16_t>(input::code::LX), x.raw, seq_++});
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   static_cast<uint16_t>(input::code::LY), y.raw, seq_++});
}

void MobileGame::post_fire(bool down) {
    fire_down_ = down;
    engine_->post({down ? input::RawKind::PadButtonDown : input::RawKind::PadButtonUp,
                   input::DeviceKind::Gamepad, 0, static_cast<uint16_t>(input::code::PadA), 0, seq_++});
}

void MobileGame::pointer(intptr_t id, Touch phase, float px, float py, float view_w, float view_h) {
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
    const uint32_t t = tick_++;
    const input::InputFrame& f = engine_->begin_tick(t);
    sink_.events.clear();
    step(world_, gs_, f, dt, &sink_);
    fx_.emit(sink_);
    if (gs_.phase == PH_Play || gs_.phase == PH_Boss) fx_.emit_trails(trails_);
    fx_.update();
    audio_.on_events(sink_);
    ach_->observe(gs_);                                 // наблюдатель: sim о нём не знает
    if ((t % 60) == 0) ach_->pump();                    // доставка — вне тика
    ach_->autosave();
    if (!lost_) draw(surface);
}

void MobileGame::draw(WGPUSurface surface) {
    const SurfaceFrame sf = acquire_frame(SurfaceSpec{surface, fmt_, fb_w_, fb_h_}, *gpu_, "mobile",
                                          surface_warned_);
    lost_ = sf.quit;
    if (!sf.texture) return;
    WGPUTextureView view = wgpuTextureCreateView(sf.texture, nullptr);
    batch_.begin();
    push_scene(batch_, world_, atlas_, sfx_);
    push_fx(batch_, fx_, atlas_);
    push_hud(batch_, world_, atlas_, gs_);
    push_screen(batch_, atlas_, gs_);
    push_toast(batch_, atlas_, ach_->toast().name.c_str(), ach_->toast().left);
    push_fire_button();
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu_->device, nullptr);
    WGPURenderPassEncoder pass = begin_clear(enc, use_bloom_ ? bloom_.hdr_view() : view,
                                             WGPUColor{0.02, 0.02, 0.07, 1.0});
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
    wgpuTextureRelease(sf.texture);
}

void MobileGame::suspend() {
    if (!inited_) return;
    ach_->pump();
    ach_->save();
}

void MobileGame::shutdown() {
    if (!inited_) return;   // идемпотентно: без парного init нечего освобождать (нет double-free)
    suspend();
    inited_ = false;
    ach_.reset();
    audio_.shutdown();
    delete engine_;
    engine_ = nullptr;
    bloom_.shutdown();
    batch_.shutdown();
    materials_.shutdown();
}

} // namespace game
