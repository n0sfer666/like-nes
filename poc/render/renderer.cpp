#include "renderer.hpp"

#include "renderer_internal.hpp"

#include <algorithm>
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

void write_lights(WGPUQueue q, WGPUBuffer buf, const SceneSnapshot& s) {
    LightsUniform lu = {};
    const int n = std::min(s.light_count, 3);
    lu.header[0] = (float)n; lu.header[1] = s.aspect; lu.header[2] = 0.09f;
    lu.ambient_col[0] = 0.30f; lu.ambient_col[1] = 0.34f; lu.ambient_col[2] = 0.50f;
    for (int i = 0; i < n; ++i) {
        const LightState& L = s.lights[i];
        lu.lights[i * 2][0] = L.x; lu.lights[i * 2][1] = L.y;
        lu.lights[i * 2][2] = L.z; lu.lights[i * 2][3] = L.radius;
        lu.lights[i * 2 + 1][0] = L.r; lu.lights[i * 2 + 1][1] = L.g;
        lu.lights[i * 2 + 1][2] = L.b; lu.lights[i * 2 + 1][3] = L.intensity;
    }
    wgpuQueueWriteBuffer(q, buf, 0, &lu, sizeof(lu));
}

WGPURenderPassEncoder begin_color(WGPUCommandEncoder enc, WGPUTextureView view,
                                  WGPUColor clear) {
    WGPURenderPassColorAttachment a = {};
    a.view = view; a.loadOp = WGPULoadOp_Clear; a.storeOp = WGPUStoreOp_Store;
    a.clearValue = clear;
    WGPURenderPassDescriptor rp = {};
    rp.colorAttachmentCount = 1; rp.colorAttachments = &a;
    return wgpuCommandEncoderBeginRenderPass(enc, &rp);
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

    WGPUBufferDescriptor ld = {};
    ld.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    ld.size = sizeof(LightsUniform);
    lights_ubo_ = wgpuDeviceCreateBuffer(device, &ld);

    build_targets();
    build_gbuffer();
    build_lighting();
    build_tonemap();
}

void Renderer::build_targets() {
    arena_.begin_frame();
    const WGPUTextureUsage rt = (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment |
                                                   WGPUTextureUsage_TextureBinding);
    albedo_ = arena_.acquire(TargetDesc{w_, h_, GBUFFER_ALBEDO_FMT, rt});
    normal_ = arena_.acquire(TargetDesc{w_, h_, GBUFFER_NORMAL_FMT, rt});
    hdr_ = arena_.acquire(TargetDesc{w_, h_, HDR_FMT, rt});
}

void Renderer::render(const SceneSnapshot& snap, WGPUTextureView out_view) {
    arena_.begin_frame();
    write_sprite(queue_, uniforms_, 0, snap.opaque, snap.aspect, 1.0f, 0.0f);
    write_lights(queue_, lights_ubo_, snap);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device_, nullptr);

    // [opaque] G-buffer.
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

    // deferred lighting → HDR (линейный).
    WGPURenderPassEncoder l = begin_color(enc, hdr_, WGPUColor{0, 0, 0, 1});
    wgpuRenderPassEncoderSetPipeline(l, lighting_pipe_);
    wgpuRenderPassEncoderSetBindGroup(l, 0, lighting_bg_, 0, nullptr);
    wgpuRenderPassEncoderDraw(l, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(l);
    wgpuRenderPassEncoderRelease(l);

    // tonemap HDR → output.
    WGPURenderPassEncoder t = begin_color(enc, out_view, WGPUColor{0, 0, 0, 1});
    wgpuRenderPassEncoderSetPipeline(t, tonemap_pipe_);
    wgpuRenderPassEncoderSetBindGroup(t, 0, tonemap_bg_, 0, nullptr);
    wgpuRenderPassEncoderDraw(t, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(t);
    wgpuRenderPassEncoderRelease(t);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

void Renderer::shutdown() {
    if (tonemap_pipe_) wgpuRenderPipelineRelease(tonemap_pipe_);
    if (tonemap_bg_) wgpuBindGroupRelease(tonemap_bg_);
    if (tonemap_bgl_) wgpuBindGroupLayoutRelease(tonemap_bgl_);
    if (lighting_pipe_) wgpuRenderPipelineRelease(lighting_pipe_);
    if (lighting_bg_) wgpuBindGroupRelease(lighting_bg_);
    if (lighting_bgl_) wgpuBindGroupLayoutRelease(lighting_bgl_);
    if (gbuffer_pipe_) wgpuRenderPipelineRelease(gbuffer_pipe_);
    if (gbuffer_bg_) wgpuBindGroupRelease(gbuffer_bg_);
    if (gbuffer_bgl_) wgpuBindGroupLayoutRelease(gbuffer_bgl_);
    if (lights_ubo_) wgpuBufferRelease(lights_ubo_);
    if (uniforms_) wgpuBufferRelease(uniforms_);
    arena_.shutdown();
    device_ = nullptr; queue_ = nullptr; sprite_ = nullptr;
}
