#include "golden_target.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "render_capture.hpp"

namespace golden_target {
namespace {

double clear_channel(uint8_t c, bool srgb) {
    const double v = c / 255.0;
    if (!srgb) return v;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

} // namespace

std::vector<uint8_t> render(GpuContext& gpu, const render::QuadRenderer& qr,
                            std::span<const render::QuadRun> runs, uint32_t w, uint32_t h,
                            const uint8_t clear[4], WGPUTextureFormat format) {
    const bool srgb = format == WGPUTextureFormat_RGBA8UnormSrgb;
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{w, h, 1};
    td.format = format;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture tex = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);
    WGPUCommandEncoderDescriptor ed = {};
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, &ed);
    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {clear_channel(clear[0], srgb), clear_channel(clear[1], srgb),
                      clear_channel(clear[2], srgb), clear[3] / 255.0};
    WGPURenderPassDescriptor pd = {};
    pd.colorAttachmentCount = 1;
    pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &pd);
    qr.draw(pass, runs);
    wgpuRenderPassEncoderEnd(pass);
    WGPUCommandBufferDescriptor cd = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(enc);
    std::vector<uint8_t> px = capture::readback_rgba(gpu.device, gpu.queue, tex, w, h);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(tex);
    return px;
}

uint32_t mismatched(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return static_cast<uint32_t>(std::max(a.size(), b.size()) / 4 + 1);
    uint32_t n = 0;
    for (std::size_t i = 0; i < a.size(); i += 4)
        if (std::memcmp(&a[i], &b[i], 4) != 0) ++n;
    return n;
}

} // namespace golden_target
