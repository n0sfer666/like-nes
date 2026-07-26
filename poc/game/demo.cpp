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
#include "draw.hpp"
#include "engine.hpp"
#include "gpu.hpp"
#include "sim.hpp"
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

} // namespace

int run_demo(const char* dir, int frames) {
    GpuContext gpu;
    if (!gpu.init(nullptr)) { gpu.shutdown(); return 1; }
    Atlas atlas = load_game_atlas(gpu.supports_bc);
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
    FxSink sink;
    input::ActionMap map = make_map();
    input::InputEngine engine(map);
    DemoDriver driver;
    Achievements ach;
    ach.init(resolve_bundle_path(), "", "");
    const fix32 dt = fix32::from_float(1.0 / 60);

    for (int t = 0; t < frames; ++t) {
        driver.drive(engine, (uint32_t)t);
        const input::InputFrame& f = engine.begin_tick((uint32_t)t, 0);
        sink.events.clear();
        step(world, gs, f, dt, &sink);
        fx.emit(sink);
        if (gs.phase == PH_Play || gs.phase == PH_Boss) fx.emit_trails(world);
        fx.update(1.0f / 60);
        ach.observe(gs);

        batch.begin();
        push_scene(batch, world, atlas);
        fx.render(batch, atlas);
        push_hud(batch, world, atlas, gs);
        push_screen(batch, atlas, gs);
        push_toast(batch, atlas, ach.toast().name.c_str(), ach.toast().left);
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
        WGPURenderPassEncoder pass = begin_clear(enc, bloom.hdr_view());   // сцена → HDR
        batch.flush(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        bloom.resolve(enc, view);                                          // bloom → offscreen
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
        wgpuQueueSubmit(gpu.queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);

        std::vector<uint8_t> px = readback_rgba(gpu.device, gpu.queue, target, VIEW_W, VIEW_H);
        if (px.empty()) { std::fprintf(stderr, "[game] readback failed at frame %d\n", t); return 1; }
        char path[512];
        std::snprintf(path, sizeof(path), "%s/frame_%04d.png", dir, t);
        write_png(path, px, VIEW_W, VIEW_H);
    }
    std::printf("[game] demo wrote %d frames to %s\n", frames, dir);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(target);
    bloom.shutdown();
    batch.shutdown();
    gpu.shutdown();
    return 0;
}

} // namespace game
