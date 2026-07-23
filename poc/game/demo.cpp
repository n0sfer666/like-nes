#include <webgpu/webgpu.h>

#include <cstdio>
#include <vector>

#include "app.hpp"
#include "art.hpp"
#include "batch.hpp"
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
    Atlas atlas = build_atlas();
    SpriteBatch batch;
    batch.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8Unorm, atlas);

    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{VIEW_W, VIEW_H, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1; td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture target = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(target, nullptr);

    flecs::world world;
    spawn(world);
    input::ActionMap map = make_map();
    input::InputEngine engine(map);
    DemoDriver driver;
    const fix32 dt = fix32::from_float(1.0 / 60);

    for (int t = 0; t < frames; ++t) {
        driver.drive(engine, (uint32_t)t);
        const input::InputFrame& f = engine.begin_tick((uint32_t)t, 0);
        step(world, f, dt);

        batch.begin();
        push_scene(batch, world, atlas);
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
        WGPURenderPassEncoder pass = begin_clear(enc, view);
        batch.flush(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
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
    batch.shutdown();
    gpu.shutdown();
    return 0;
}

} // namespace game
