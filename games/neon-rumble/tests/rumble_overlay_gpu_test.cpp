#include <array>
#include <cstdio>

#include "render_capture.hpp"
#include "rumble_gpu_frame.hpp"

namespace {

struct Pixels { bool bars = false, scene = false, overlay = false; };

Pixels render_frame(GpuContext& gpu, framework::graphics::PixelRect scene_clip,
                    framework::graphics::PixelRect overlay_clip) {
    render::QuadRenderer renderer;
    constexpr uint8_t WHITE[] = {255, 255, 255, 255};
    if (!renderer.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8Unorm, 2) ||
        !renderer.add_texture(WHITE, 1, 1)) return {};
    const std::array<render::Quad, 2> quads{{
        {0, 0, 64, 64, 0, 0, 1, 1, 0xFF0000FFu, 0},
        {0, 48, 8, 8, 0, 0, 1, 1, 0x00FF00FFu, 0}}};
    if (!renderer.upload(quads, 64, 64)) return {};
    WGPUTextureDescriptor td{};
    td.dimension = WGPUTextureDimension_2D;
    td.size = {64, 64, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture texture = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    const render::QuadRun scene{0, 1, 0}, overlay{1, 1, 0};
    rumble::submit_quads(gpu, view, {0, 0, 0, 1}, scene_clip, renderer, {&scene, 1},
                         rumble::QuadOverlay{overlay_clip, {&overlay, 1}});
    const std::vector<uint8_t> pixels = capture::readback_rgba(gpu.device, gpu.queue, texture, 64, 64);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    if (pixels.size() != 64 * 64 * 4) return {};
    const auto pixel = [&](uint32_t x, uint32_t y, std::array<uint8_t, 4> want) {
        const size_t offset = (y * 64 + x) * 4;
        for (size_t c = 0; c < 4; ++c) if (pixels[offset + c] != want[c]) return false;
        return true;
    };
    return {pixel(4, 4, {0, 0, 0, 255}), pixel(32, 4, {255, 0, 0, 255}), pixel(4, 52, {0, 255, 0, 255})};
}

} // namespace

int main() {
    GpuContext gpu;
    if (!gpu.init(nullptr)) { std::puts("rumble-overlay-gpu: FAIL no adapter"); return 1; }
    const framework::graphics::PixelRect scene{16, 0, 32, 64}, screen{0, 0, 64, 64};
    const Pixels normal = render_frame(gpu, scene, screen);
    const Pixels clipped_overlay = render_frame(gpu, scene, scene);
    const Pixels escaped_scene = render_frame(gpu, screen, screen);
    if (!normal.bars) std::puts("  FAIL: scene leaked into the viewport bar");
    if (!normal.scene) std::puts("  FAIL: scene did not draw inside its viewport");
    if (!normal.overlay) std::puts("  FAIL: overlay did not draw outside the game viewport");
    const bool controls = clipped_overlay.bars && clipped_overlay.scene && !clipped_overlay.overlay &&
        !escaped_scene.bars && escaped_scene.scene && escaped_scene.overlay;
    if (!controls) std::puts("  FAIL: wrong clip controls were not distinguished");
    const bool ok = normal.bars && normal.scene && normal.overlay && controls;
    gpu.shutdown();
    std::puts(ok ? "rumble-overlay-gpu: PASS" : "rumble-overlay-gpu: FAIL");
    return ok ? 0 : 1;
}
