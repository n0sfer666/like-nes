#if defined(RUMBLE_RESIM_BENCH)
#include "rumble_bench.hpp"

std::unique_ptr<mobile::Game> mobile::make_game() { return rumble::make_bench_game(); }
#else
#include "rumble_mobile.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include "gpu.hpp"
#include "mobile_host.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_demo.hpp"
#include "rumble_join_report.hpp"
#include "rumble_gpu_frame.hpp"
#include "rumble_report.hpp"

namespace rumble {

namespace {

// Драка шагает тиком 1/60 с, а не кадром: экран на 90 или 120 Гц иначе ускорил бы её в полтора-два
// раза. Долгий кадр (возврат из фона, отладчик) догоняется не больше чем на MAX_STEPS тиков — остаток
// отбрасывается, чтобы один медленный кадр не растянулся в лавину следующих.
constexpr double TICK = 1.0 / 60.0;
constexpr int MAX_STEPS = 4;

} // namespace

bool MobileRumble::init(GpuContext& gpu) {
    if (inited_) return true;
    gpu_ = &gpu;
    if (!report_library_bundle(mobile::asset_path("library.bundle"))) return false;
    const std::string path = mobile::asset_path("game.bundle");
    if (!file_.open(path) || !bundle_.open(file_.data(), file_.size(), false)) {
        std::fprintf(stderr, "neon-rumble: cannot open %s\n", path.c_str());
        return false;
    }
    const bool waves = mobile::has_arg("--wave");
    wave_demo_ = {};
    if (!credits_.open(bundle_) || !menu_.init("level1", waves) || !flow_.open()) return false;
    // Мобильный гейт входит в выбранный режим сразу, без desktop-меню.
    flow_.enter();
    flow_.apply();
    if (flow_.play() == nullptr || !open_controls()) return false;
    std::fprintf(stderr, "[game] level mode %s\n", waves ? "wave" : "arena");
    screen_ = std::make_unique<Screen>();
    inited_ = true;
    return true;
}

bool MobileRumble::bind() {
    if (bind_scene(quads_, *gpu_, spec_.format, flow_, credits_, clear_)) return true;
    std::fprintf(stderr, "neon-rumble: quad renderer or scene texture upload failed\n");
    quads_.shutdown();
    return false;
}

void MobileRumble::build_targets(uint32_t fb_w, uint32_t fb_h) {
    release_targets();
    if (fb_w == 0 || fb_h == 0) return;
    spec_ = SurfaceSpec{surface_, WGPUTextureFormat_Undefined, fb_w, fb_h};
    spec_.format = configure_surface(surface_, gpu_->adapter, gpu_->device, fb_w, fb_h);
    warned_ = lost_ = false;
    targets_ = bind();
}

void MobileRumble::release_targets() {
    if (!targets_) return;
    targets_ = false;
    quads_.shutdown();
}

// Улика непрерывности для гейта симулятора: хеш драки второго attach равен хешу последнего detach —
// уход в фон снял поверхность, а драка пережила его, а не началась заново.
void MobileRumble::report_surface(const char* what) const {
    const Play* play = flow_.play();
    std::fprintf(stderr, "[game] surface %s tick=%u world=%016llx\n", what, play != nullptr ? play->scene.ticks : 0,
                 static_cast<unsigned long long>(play != nullptr ? play->brawl.hash() : 0));
}

bool MobileRumble::attach_surface(WGPUSurface surface, uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) {
    if (!inited_ || !surface) return false;
    surface_ = surface;
    fit_touch(fb_w, fb_h, safe);
    build_targets(fb_w, fb_h);
    const std::string what = "attached #" + std::to_string(++attaches_);
    report_surface(what.c_str());
    return true;
}

void MobileRumble::detach_surface() {
    if (!inited_) return;
    cancel();
    release_targets();
    surface_ = nullptr;
    report_surface("detached");
}

// Поворот меняет только размер: формат поверхности тот же, и текстуры сцены перезаливать незачем.
void MobileRumble::resize(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) {
    fit_touch(fb_w, fb_h, safe);
    if (!surface_) return;
    if (!targets_ || fb_w == 0 || fb_h == 0) {
        build_targets(fb_w, fb_h);
        return;
    }
    spec_.width = fb_w;
    spec_.height = fb_h;
    reconfigure_surface(spec_, gpu_->device);
    warned_ = lost_ = false;
}

bool MobileRumble::wants_frames() const {
    return inited_ && targets_ && !lost_ && !gpu_->device_lost.load();
}

// Порядок шага — как у run_headless: клавиши P2 до тика лобби, команда P1 поверх собранной им, места
// до шага — до run, отчёты — после него.
void MobileRumble::step() {
    Play* play = flow_.play();
    if (play == nullptr) {
        hotseat_.engine().drain();
        flow_.run(ticks_);
        return;
    }
    const uint32_t t = play->scene.ticks;
    if (demo_ && menu_.item != MenuItem::Wave) demo_p2(hotseat_.engine(), t + 1);
    const bool run = hotseat_.tick(t + 1, play->brawl.players);
    static_assert(input::MAX_DEVICES <= 32);
    uint32_t pads = 0;
    for (int slot = 0; slot < input::MAX_DEVICES; ++slot)
        if (hotseat_.engine().device().pad_connected[slot]) pads |= 1u << slot;
    touch_.update_pads(pads);
    if (!run) return;
    if (demo_) play->brawl.players[PLAYER] = menu_.item == MenuItem::Wave ?
        wave_demo_.next(play->brawl, t) : demo_p1(t);
    const framework::brawl::Seats was = play->brawl.seats;
    flow_.run(ticks_);
    report_step(play->brawl, play->scene.ticks);
    report_joins(play->brawl, was, play->scene.ticks);
}

void MobileRumble::frame(double dt) {
    if (!inited_) return;
    if (flow_.apply()) {
        cancel();
        screen_->reset();
        if (targets_) targets_ = bind();
    }
    lag_ = std::min(lag_ + dt, TICK * MAX_STEPS);
    while (lag_ >= TICK) {
        lag_ -= TICK;
        step();
        ++ticks_;
    }
    if (targets_ && !lost_) draw();
}

void MobileRumble::shutdown() {
    if (!inited_) return;
    cancel();
    inited_ = false;
    release_targets();
    surface_ = nullptr;
}

} // namespace rumble

std::unique_ptr<mobile::Game> mobile::make_game() { return std::make_unique<rumble::MobileRumble>(); }

#endif
