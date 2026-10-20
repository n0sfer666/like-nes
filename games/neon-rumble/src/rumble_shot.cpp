#include "rumble_shot.hpp"

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "render_capture.hpp"
#include "rumble_gpu_frame.hpp"

namespace rumble {

namespace {

bool is_bgra(WGPUTextureFormat format) {
    return format == WGPUTextureFormat_BGRA8Unorm || format == WGPUTextureFormat_BGRA8UnormSrgb;
}

bool is_rgba(WGPUTextureFormat format) {
    return format == WGPUTextureFormat_RGBA8Unorm || format == WGPUTextureFormat_RGBA8UnormSrgb;
}

} // namespace

bool shoot(const GpuContext& gpu, const ShotTarget& target, const render::QuadRenderer& quads,
           std::span<const render::QuadRun> runs, const std::string& path) {
    if (!is_bgra(target.format) && !is_rgba(target.format)) {
        std::fprintf(stderr, "neon-rumble: surface format 0x%x is not 8-bit RGBA or BGRA, no shot\n",
                     static_cast<unsigned>(target.format));
        return false;
    }
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{target.width, target.height, 1};
    td.format = target.format;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture tex = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);
    submit_quads(gpu, view, target.clear, target.shown, quads, runs);
    std::vector<uint8_t> px = capture::readback_rgba(gpu.device, gpu.queue, tex, target.width, target.height);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(tex);
    if (px.empty()) return false;
    if (is_bgra(target.format))
        for (size_t i = 0; i + 3 < px.size(); i += 4) std::swap(px[i], px[i + 2]);
    return capture::write_png(path.c_str(), px, target.width, target.height);
}

} // namespace rumble
