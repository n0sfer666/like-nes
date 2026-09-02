#include "material_frame.hpp"

#include "capture.hpp"
#include "gpu.hpp"

namespace matgold {

std::vector<uint8_t> render_frame(GpuContext& gpu, Scene& scene, uint32_t w, uint32_t h,
                                  uint32_t& draws) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{w, h, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
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
    att.clearValue = WGPUColor{0.05, 0.06, 0.09, 1.0};
    WGPURenderPassDescriptor pd = {};
    pd.colorAttachmentCount = 1;
    pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &pd);
    draws = scene.draw(pass);
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

} // namespace matgold
