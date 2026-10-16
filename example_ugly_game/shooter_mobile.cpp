#include "shooter_mobile.hpp"

#include <cstdio>

#include "assets_path.hpp"
#include "draw.hpp"
#include "game_sim.hpp"
#include "mobile_host.hpp"
#include "sim_hash.hpp"

namespace game {

bool MobileGame::init(GpuContext& gpu) {
    if (inited_) return true;
    gpu_ = &gpu;
    controls_ = Controls{};
    if (!load_controls(controls_)) {
        std::fprintf(stderr, "[game] controls unavailable\n");
        return false;
    }
    atlas_ = load_game_atlas(gpu.supports_bc);
    spawn(world_, gs_);
    trails_ = make_trail_query(world_);
    engine_ = std::make_unique<input::InputEngine>(controls_.map);
    touch_.bind(engine_.get());
    demo_ = mobile::has_arg("--demo");
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

// Пересборка целиком, а не по частям: формат сцены зависит от того, поднялся ли bloom, а от
// формата — материалы и батч. Цена — сотни миллисекунд на редкий attach или поворот.
void MobileGame::build_targets(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) {
    release_targets();
    if (fb_w == 0 || fb_h == 0) return;
    fb_w_ = fb_w; fb_h_ = fb_h;
    surface_warned_ = lost_ = false;
    fmt_ = configure_surface(surface_, gpu_->adapter, gpu_->device, fb_w, fb_h);
    use_bloom_ = bloom_.init(gpu_->device, gpu_->queue, fmt_, fb_w, fb_h);
    const WGPUTextureFormat scene_fmt = use_bloom_ ? WGPUTextureFormat_RGBA16Float : fmt_;
    const bool mat_ok = materials_.init(gpu_->device, gpu_->queue, scene_fmt,
                                        resolve_asset("library.bundle").c_str());
    // bind зовётся и при отказе: он же отвязывает sfx_ от материалов прошлой поверхности.
    const bool have_fx = sfx_.bind(mat_ok ? &materials_ : nullptr);
    std::printf("[game] materials: %s (%u pipeline(s), %u fallback(s))\n", have_fx ? "on" : "off",
                materials_.pipelines_created(), materials_.fallbacks());
    batch_.init(gpu_->device, gpu_->queue, scene_fmt, atlas_, have_fx ? &materials_ : nullptr);
    targets_ = true;

    const double sa = static_cast<double>(fb_w) / fb_h, wa = static_cast<double>(VIEW_W) / VIEW_H;
    const uint32_t world_w = sa >= wa ? static_cast<uint32_t>(VIEW_H * sa) : VIEW_W;
    const uint32_t world_h = sa >= wa ? VIEW_H : static_cast<uint32_t>(VIEW_W / sa);
    batch_.set_viewport(world_w, world_h);
    vw_ = static_cast<float>(world_w); vh_ = static_cast<float>(world_h);
    const float scale = safe.scale > 0 ? safe.scale : 1.0f;
    touch_.set_view(static_cast<float>(fb_w) / scale, static_cast<float>(fb_h) / scale);
}

void MobileGame::release_targets() {
    if (!targets_) return;
    targets_ = false;
    sfx_.bind(nullptr);
    bloom_.shutdown();
    batch_.shutdown();
    materials_.shutdown();
}

bool MobileGame::attach_surface(WGPUSurface surface, uint32_t fb_w, uint32_t fb_h,
                                const mobile::SafeArea& safe) {
    if (!inited_ || !surface) return false;
    surface_ = surface;
    build_targets(fb_w, fb_h, safe);
    // Улика непрерывности для гейта симулятора: мир второго attach равен миру последнего detach —
    // уход в фон снял поверхность, а мир пережил его, а не начался заново. Тик один его не судит:
    // мир, пересозданный в attach, сохранил бы счётчик кадров.
    std::fprintf(stderr, "[game] surface attached #%u tick=%u world=%016llx\n", ++attaches_, tick_,
                 static_cast<unsigned long long>(sim_hash(world_, gs_)));
    return true;
}

void MobileGame::detach_surface() {
    if (!inited_) return;
    touch_.cancel();
    release_targets();
    surface_ = nullptr;
    std::fprintf(stderr, "[game] surface detached tick=%u world=%016llx\n", tick_,
                 static_cast<unsigned long long>(sim_hash(world_, gs_)));
}

bool MobileGame::wants_frames() const {
    return inited_ && targets_ && !lost_ && !gpu_->device_lost.load();
}

void MobileGame::resize(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) {
    if (surface_) build_targets(fb_w, fb_h, safe);
}

// dt не читается: симуляция шагает фиксированным тиком 1/60 на кадр, как до спеки #26.
void MobileGame::frame(double) {
    if (!inited_) return;
    if (demo_ && touch_.idle()) touch_.demo(tick_);
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
    if (targets_ && !lost_) draw();
}

void MobileGame::draw() {
    const SurfaceFrame sf = acquire_frame(SurfaceSpec{surface_, fmt_, fb_w_, fb_h_}, *gpu_, "mobile",
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
    touch_.push_button(batch_, atlas_.star, vw_, vh_);
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
    wgpuSurfacePresent(surface_);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(sf.texture);
}

std::string MobileGame::touch_state() const {
    char buf[64];
    std::snprintf(buf, sizeof buf, "stick=%ld fire=%ld", static_cast<long>(touch_.stick()),
                  static_cast<long>(touch_.fire()));
    return buf;
}

// Уход в фон: система убивает фоновый процесс без dealloc и onDestroy, и статы достижений,
// копящиеся до автосейва, пропали бы. Сохраняет, ничего не освобождая.
void MobileGame::suspend() {
    if (!inited_) return;
    ach_->pump();
    ach_->save();
}

void MobileGame::shutdown() {
    if (!inited_) return;   // идемпотентно: без парного init нечего освобождать (нет double-free)
    suspend();
    inited_ = false;
    release_targets();
    surface_ = nullptr;
    ach_.reset();
    audio_.shutdown();
    engine_.reset();
}

} // namespace game

std::unique_ptr<mobile::Game> mobile::make_game() { return std::make_unique<game::MobileGame>(); }
