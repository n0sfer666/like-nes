#include "wgpu_imgui.hpp"
#include "imgui.h"
#include "backends/imgui_impl_wgpu.h"

namespace wgpu_imgui {

void draw_into(const GpuContext& gpu, WGPUTextureView view, WGPUColor clear) {
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
}

Presented present(const GpuContext& gpu, const SurfaceSpec& spec, const char* tag, bool& warned,
                  WGPUColor clear) {
    const SurfaceFrame frame = acquire_frame(spec, gpu, tag, warned);
    if (!frame.texture) return frame.quit ? Presented::Lost : Presented::Skipped;
    WGPUTextureView view = wgpuTextureCreateView(frame.texture, nullptr);
    draw_into(gpu, view, clear);
    wgpuSurfacePresent(spec.surface);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(frame.texture);
    return Presented::Drawn;
}

} // namespace wgpu_imgui
