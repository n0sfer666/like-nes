#include "rumble_gpu_frame.hpp"

#include <cmath>

#include "rumble_layers.hpp"
#include "rumble_roster.hpp"

namespace rumble {

using framework::graphics::PixelRect;

namespace {

double channel(uint32_t rgba, int shift, bool srgb) {
    const double v = static_cast<double>((rgba >> shift) & 0xffu) / 255.0;
    if (!srgb) return v;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

} // namespace

WGPUColor background(const Level& level, WGPUTextureFormat format) {
    const uint32_t rgba = level.map.row->background_rgba;
    const bool srgb = render::quad_target_srgb(format);
    return {channel(rgba, 24, srgb), channel(rgba, 16, srgb), channel(rgba, 8, srgb), 1.0};
}

void draw_frame(const GpuContext& gpu, WGPUSurface surface, WGPUTextureView view, WGPUColor clear,
                const PixelRect& shown, const render::QuadRenderer& quads,
                std::span<const render::QuadRun> runs) {
    WGPURenderPassColorAttachment color = {};
    color.view = view;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = clear;

    WGPURenderPassDescriptor rp = {};
    rp.label = "neon-rumble-layers";
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &color;

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    wgpuRenderPassEncoderSetScissorRect(pass, static_cast<uint32_t>(shown.x), static_cast<uint32_t>(shown.y),
                                        shown.w, shown.h);
    quads.draw(pass, runs);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuSurfacePresent(surface);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

// Текстуры уровня уходят в GPU один раз, в порядке `Level::guids`: этот порядок и есть номер
// материала спрайта, который `draw_layer` выдаёт по guid. Листы бойцов в порядке `ROSTER`, сплошная
// текстура оверлея и атлас шрифта титров встают сразу за ними — номера дают `sheet_texture`,
// `solid_texture` и `atlas_texture`.
bool init_quads(render::QuadRenderer& quads, const GpuContext& gpu, WGPUTextureFormat format,
                const Level& level, const Fighters& fighters, const Credits& credits) {
    static constexpr uint8_t SOLID[4] = {255, 255, 255, 255};
    if (!quads.init(gpu.device, gpu.queue, format, Layers::CAPACITY)) return false;
    for (uint32_t i = 0; i < level.texture_count; ++i)
        if (!quads.add_texture(level.pixels[i].pixels, level.pixels[i].width,
                               level.pixels[i].height))
            return false;
    for (const Fighter& f : fighters)
        if (!quads.add_texture(f.sheet.pixels, f.sheet.width, f.sheet.height)) return false;
    return quads.add_texture(SOLID, 1, 1)
           && quads.add_texture(credits.atlas.pixels, credits.atlas.width, credits.atlas.height);
}

} // namespace rumble
