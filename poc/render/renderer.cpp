#include "renderer.hpp"

#include "renderer_internal.hpp"

#include <cstring>

namespace {

void write_sprite(WGPUQueue q, WGPUBuffer buf, uint32_t offset, const SpriteXform& x,
                  float aspect, float alpha, float emissive) {
    SpriteUniform u = {};
    u.pos[0] = x.x; u.pos[1] = x.y;
    u.scale = x.scale; u.rot = x.rot;
    u.aspect = aspect; u.alpha = alpha; u.emissive = emissive;
    wgpuQueueWriteBuffer(q, buf, offset, &u, sizeof(u));
}

} // namespace

void Renderer::init(WGPUDevice device, WGPUQueue queue, const Sprite& sprite,
                    WGPUTextureFormat out_format, uint32_t w, uint32_t h) {
    device_ = device; queue_ = queue; sprite_ = &sprite;
    out_format_ = out_format; w_ = w; h_ = h;
    arena_.init(device);

    WGPUBufferDescriptor bd = {};
    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    bd.size = uniform_stride_ * 4;
    uniforms_ = wgpuDeviceCreateBuffer(device, &bd);

    build_targets();
    build_gbuffer();
    build_preview();
}

void Renderer::build_targets() {
    arena_.begin_frame();
    const WGPUTextureUsage rt = (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment |
                                                   WGPUTextureUsage_TextureBinding);
    albedo_ = arena_.acquire(TargetDesc{w_, h_, GBUFFER_ALBEDO_FMT, rt});
    normal_ = arena_.acquire(TargetDesc{w_, h_, GBUFFER_NORMAL_FMT, rt});
}

void Renderer::render(const SceneSnapshot& snap, WGPUTextureView out_view) {
    arena_.begin_frame();
    (void)arena_.acquire(TargetDesc{w_, h_, GBUFFER_ALBEDO_FMT,
        (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding)});
    (void)arena_.acquire(TargetDesc{w_, h_, GBUFFER_NORMAL_FMT,
        (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding)});

    write_sprite(queue_, uniforms_, 0, snap.opaque, snap.aspect, 1.0f, 0.0f);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device_, nullptr);

    WGPURenderPassColorAttachment gb[2] = {};
    gb[0].view = albedo_; gb[0].loadOp = WGPULoadOp_Clear; gb[0].storeOp = WGPUStoreOp_Store;
    gb[0].clearValue = WGPUColor{0, 0, 0, 0};
    gb[1].view = normal_; gb[1].loadOp = WGPULoadOp_Clear; gb[1].storeOp = WGPUStoreOp_Store;
    gb[1].clearValue = WGPUColor{0.5, 0.5, 1.0, 1.0};
    WGPURenderPassDescriptor gbpass = {};
    gbpass.colorAttachmentCount = 2; gbpass.colorAttachments = gb;
    WGPURenderPassEncoder p = wgpuCommandEncoderBeginRenderPass(enc, &gbpass);
    wgpuRenderPassEncoderSetPipeline(p, gbuffer_pipe_);
    uint32_t off = 0;
    wgpuRenderPassEncoderSetBindGroup(p, 0, gbuffer_bg_, 1, &off);
    wgpuRenderPassEncoderSetVertexBuffer(p, 0, sprite_->vbo, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(p, sprite_->ibo, WGPUIndexFormat_Uint16, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(p, SPRITE_INDEX_COUNT, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(p);
    wgpuRenderPassEncoderRelease(p);

    WGPURenderPassColorAttachment oc = {};
    oc.view = out_view; oc.loadOp = WGPULoadOp_Clear; oc.storeOp = WGPUStoreOp_Store;
    oc.clearValue = WGPUColor{0.02, 0.02, 0.03, 1.0};
    WGPURenderPassDescriptor opass = {};
    opass.colorAttachmentCount = 1; opass.colorAttachments = &oc;
    WGPURenderPassEncoder q = wgpuCommandEncoderBeginRenderPass(enc, &opass);
    wgpuRenderPassEncoderSetPipeline(q, preview_pipe_);
    wgpuRenderPassEncoderSetBindGroup(q, 0, preview_bg_, 0, nullptr);
    wgpuRenderPassEncoderDraw(q, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(q);
    wgpuRenderPassEncoderRelease(q);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

void Renderer::shutdown() {
    if (preview_pipe_) wgpuRenderPipelineRelease(preview_pipe_);
    if (preview_bg_) wgpuBindGroupRelease(preview_bg_);
    if (preview_bgl_) wgpuBindGroupLayoutRelease(preview_bgl_);
    if (gbuffer_pipe_) wgpuRenderPipelineRelease(gbuffer_pipe_);
    if (gbuffer_bg_) wgpuBindGroupRelease(gbuffer_bg_);
    if (gbuffer_bgl_) wgpuBindGroupLayoutRelease(gbuffer_bgl_);
    if (uniforms_) wgpuBufferRelease(uniforms_);
    arena_.shutdown();
    device_ = nullptr; queue_ = nullptr; sprite_ = nullptr;
}
