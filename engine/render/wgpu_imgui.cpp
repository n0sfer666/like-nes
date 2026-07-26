#include "wgpu_imgui.hpp"
#include "imgui.h"
#include "backends/imgui_impl_wgpu.h"

namespace wgpu_imgui {

WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d, uint32_t w, uint32_t h) {
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(s, a, &caps);
    WGPUTextureFormat fmt = caps.formatCount > 0 ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = d; cfg.format = fmt; cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto; cfg.width = w; cfg.height = h;
    cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(s, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return fmt;
}

bool present(const GpuContext& gpu, WGPUSurface surface, WGPUColor clear) {
    WGPUSurfaceTexture stex = {};
    wgpuSurfaceGetCurrentTexture(surface, &stex);
    if (stex.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        if (stex.texture) wgpuTextureRelease(stex.texture);
        return false;
    }
    WGPUTextureView view = wgpuTextureCreateView(stex.texture, nullptr);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassColorAttachment color = {};
    color.view = view; color.loadOp = WGPULoadOp_Clear; color.storeOp = WGPUStoreOp_Store;
    color.clearValue = clear;
    WGPURenderPassDescriptor rp = {};
    rp.colorAttachmentCount = 1; rp.colorAttachments = &color;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuSurfacePresent(surface);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(stex.texture);
    return true;
}

} // namespace wgpu_imgui
