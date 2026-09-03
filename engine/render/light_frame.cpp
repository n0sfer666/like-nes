#include "light_frame.hpp"

#include "capture.hpp"
#include "gpu.hpp"
#include "material_frame.hpp"

namespace lightgold {
namespace {

WGPUTexture make_target(WGPUDevice device, uint32_t w, uint32_t h) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{w, h, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding |
               WGPUTextureUsage_CopySrc;
    return wgpuDeviceCreateTexture(device, &td);
}

} // namespace

std::vector<uint8_t> render_frame(GpuContext& gpu, matgold::Scene& scene, lightgfx::Pass* pass,
                                  normgfx::Pass* normals, uint32_t w, uint32_t h,
                                  uint32_t& draws) {
    WGPUTexture albedo = make_target(gpu.device, w, h);
    WGPUTextureView albedo_view = wgpuTextureCreateView(albedo, nullptr);
    WGPUTexture lit = pass ? make_target(gpu.device, w, h) : nullptr;
    WGPUTextureView lit_view = lit ? wgpuTextureCreateView(lit, nullptr) : nullptr;
    // Буфер нормалей не создаётся, когда прохода освещения нет: выключенный проход обязан не
    // оставлять в графе НИЧЕГО, и лишняя текстура была бы ровно тем следом, которого не должно быть.
    const bool with_normals = pass != nullptr && normals != nullptr;
    WGPUTexture nrm = with_normals ? make_target(gpu.device, w, h) : nullptr;
    WGPUTextureView nrm_view = nrm ? wgpuTextureCreateView(nrm, nullptr) : nullptr;

    WGPUCommandEncoderDescriptor ed = {};
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, &ed);

    WGPURenderPassColorAttachment att = {};
    att.view = albedo_view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = matgold::CLEAR;
    WGPURenderPassDescriptor pd = {};
    pd.colorAttachmentCount = 1;
    pd.colorAttachments = &att;
    WGPURenderPassEncoder scene_pass = wgpuCommandEncoderBeginRenderPass(enc, &pd);
    draws = scene.draw(scene_pass);
    wgpuRenderPassEncoderEnd(scene_pass);
    wgpuRenderPassEncoderRelease(scene_pass);

    if (with_normals) normals->run(enc, nrm_view);
    if (pass) pass->run(enc, lit_view, albedo_view, nrm_view);

    WGPUCommandBufferDescriptor cd = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);

    std::vector<uint8_t> px =
        capture::readback_rgba(gpu.device, gpu.queue, lit ? lit : albedo, w, h);
    if (nrm_view) wgpuTextureViewRelease(nrm_view);
    if (nrm) wgpuTextureRelease(nrm);
    if (lit_view) wgpuTextureViewRelease(lit_view);
    if (lit) wgpuTextureRelease(lit);
    wgpuTextureViewRelease(albedo_view);
    wgpuTextureRelease(albedo);
    return px;
}

} // namespace lightgold
