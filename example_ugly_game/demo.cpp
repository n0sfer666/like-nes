#include <webgpu/webgpu.h>

#include <cstdio>
#include <vector>

#include "achievements.hpp"
#include "app.hpp"
#include "fx.hpp"
#include "bloom.hpp"
#include "art.hpp"
#include "batch.hpp"
#include "assets_path.hpp"
#include "capture.hpp"
#include "frame_golden.hpp"
#include "draw.hpp"
#include "engine.hpp"
#include "gpu.hpp"
#include "gpu_env.hpp"
#include "sim.hpp"
#include "input_setup.hpp"
#include "world.hpp"

namespace game {
namespace {

struct DemoDriver {
    uint32_t seq = 0;
    uint32_t held = 0;
    void key(input::InputEngine& e, uint16_t code, bool down) {
        e.post({down ? input::RawKind::KeyDown : input::RawKind::KeyUp,
                input::DeviceKind::Keyboard, 0, code, 0, seq++});
    }
    void drive(input::InputEngine& e, uint32_t t) {
        if (t == 60) key(e, 32, true);   // Space @t=60: intro виден ~1с, затем Play + авто-огонь
        static const uint16_t K[4] = {68, 65, 87, 83};
        static const uint32_t PAT[8] = {0x1, 0x4, 0x2, 0x8, 0x5, 0x6, 0xA, 0x9};
        const uint32_t want = PAT[(t / 45) % 8];
        for (int i = 0; i < 4; ++i) {
            const uint32_t bit = 1u << i;
            if ((want & bit) && !(held & bit)) key(e, K[i], true);
            if (!(want & bit) && (held & bit)) key(e, K[i], false);
        }
        held = want;
    }
};

int render_run(const DemoOptions& opt, std::vector<uint8_t>& last) {
    GpuContext gpu;
    apply_gpu_env(gpu);
    if (!gpu.init(nullptr)) {
        // Шаг CI «Game — headless demo» — жёсткий гейт на трёх ОС, и молчаливый выход из него
        // неотличим от «игра не анимируется». Причина отказа должна быть в логе раннера.
        std::fprintf(stderr, "demo: no WebGPU adapter/device on this machine\n");
        gpu.shutdown();
        return 1;
    }
    Atlas atlas = load_game_atlas(gpu.supports_bc);
    // Эталон испечён с БЕЙКНУТОГО атласа, а откат `load_game_atlas` рисует пиксели процедурно —
    // это другая картинка, а не расхождение бэкендов. Названная тут, причина стоит в логе; без
    // неё гейт 2 краснел бы непрозрачным «пиксели не сошлись» и обвинял бы рендер вместо ассетов.
    if ((opt.golden || opt.selftest) && atlas.bc7.empty()) {
        std::fprintf(stderr, "[game] golden: procedural atlas (no baked BC7) - nothing to compare\n");
        gpu.shutdown();
        return 1;
    }
    SpriteBatch batch;
    batch.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA16Float, atlas);   // → HDR (bloom)
    Bloom bloom;
    if (!bloom.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8Unorm, VIEW_W, VIEW_H)) {
        std::fprintf(stderr, "bloom init failed\n"); gpu.shutdown(); return 1;
    }

    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{VIEW_W, VIEW_H, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1; td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture target = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(target, nullptr);

    flecs::world world;
    GameState gs;
    spawn(world, gs);
    Fx fx;
    const TrailQuery trails = make_trail_query(world);
    FxSink sink;
    Controls controls;
    if (!load_controls(controls)) { std::fprintf(stderr, "controls unavailable\n"); return 1; }
    input::InputEngine engine(controls.map);
    DemoDriver driver;
    Achievements ach;
    ach.init(resolve_bundle_path(), "", "");
    const fix32 dt = fix32::from_float(1.0 / 60);

    for (int t = 0; t < opt.frames; ++t) {
        driver.drive(engine, (uint32_t)t);
        const input::InputFrame& f = engine.begin_tick((uint32_t)t, 0);
        sink.events.clear();
        step(world, gs, f, dt, &sink);
        fx.emit(sink);
        if (gs.phase == PH_Play || gs.phase == PH_Boss) fx.emit_trails(trails);
        fx.update();
        ach.observe(gs);

        batch.begin();
        push_scene(batch, world, atlas);
        push_fx(batch, fx, atlas);
        push_hud(batch, world, atlas, gs);
        push_screen(batch, atlas, gs);
        push_toast(batch, atlas, ach.toast().name.c_str(), ach.toast().left);
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
        WGPURenderPassEncoder pass = begin_clear(enc, bloom.hdr_view(), WGPUColor{0.02, 0.02, 0.07, 1.0});   // сцена → HDR
        batch.flush(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        bloom.resolve(enc, view);                                          // bloom → offscreen
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
        wgpuQueueSubmit(gpu.queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);

        last = readback_rgba(gpu.device, gpu.queue, target, VIEW_W, VIEW_H);
        if (last.empty()) { std::fprintf(stderr, "[game] readback failed at frame %d\n", t); return 1; }
        if (opt.dir) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/frame_%04d.png", opt.dir, t);
            write_png(path, last, VIEW_W, VIEW_H);
        }
    }
    if (opt.dir) std::printf("[game] demo wrote %d frames to %s\n", opt.frames, opt.dir);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(target);
    bloom.shutdown();
    batch.shutdown();
    gpu.shutdown();
    return 0;
}

int report(const char* what, const FrameDiff& d, FrameTolerance tol) {
    std::printf("[game] golden %s: mean %.5f max %.5f over-eps %.4f%% (eps %.3f, frac %.2f%%, cap %.2f)\n",
                what, d.mean_abs, d.max_abs, d.frac_over * 100.0, tol.pixel_eps,
                tol.frac_tol * 100.0, tol.max_cap);
    std::printf("[game] golden %s: %s\n", what, d.pass ? "PASS" : "FAIL");
    return d.pass ? 0 : 1;
}

} // namespace

int run_demo(const DemoOptions& opt) {
    std::vector<uint8_t> px;
    if (render_run(opt, px) != 0) return 1;
    if (px.empty()) { std::fprintf(stderr, "[game] golden: no frame rendered\n"); return 1; }
    int rc = 0;

    // Оба утверждения — над ОДНОЙ парой прогонов: сверка с эталоном чужого бэкенда отвечает на
    // «узнаваем ли кадр», повторяемость — на «повторяем ли мы сами себя», и первая без второй
    // молчит про собственный дрейф, попавший в допуск чужого.
    if (opt.selftest) {
        if (!comparator_refuses_spoiled(px)) {
            std::printf("[game] golden control: FAIL (spoiled frame passed the comparison)\n");
            return 1;
        }
        std::printf("[game] golden control: PASS (blot and drift refused)\n");
        std::vector<uint8_t> again;
        if (render_run(opt, again) != 0) return 1;
        rc |= report("repeat", compare_frames(px, again, TOL_SAME_BACKEND), TOL_SAME_BACKEND);
    }
    if (!opt.golden) return rc;
    if (opt.update) {
        write_png(opt.golden, px, VIEW_W, VIEW_H);
        std::printf("[game] golden: wrote %s (%ux%u)\n", opt.golden, VIEW_W, VIEW_H);
        return rc;
    }
    std::vector<uint8_t> ref;
    uint32_t rw = 0, rh = 0;
    if (!read_png_rgba(opt.golden, ref, rw, rh)) {
        std::fprintf(stderr, "[game] golden: cannot read %s\n", opt.golden);
        return 1;
    }
    if (rw != VIEW_W || rh != VIEW_H) {
        std::fprintf(stderr, "[game] golden: %s is %ux%u, frame is %ux%u\n", opt.golden, rw, rh,
                     VIEW_W, VIEW_H);
        return 1;
    }
    rc |= report("frame", compare_frames(px, ref, TOL_CROSS_BACKEND), TOL_CROSS_BACKEND);
    if (rc != 0) {
        write_png("golden_actual.png", px, VIEW_W, VIEW_H);
        std::printf("[game] golden: rendered frame saved as golden_actual.png\n");
    }
    return rc;
}

} // namespace game
