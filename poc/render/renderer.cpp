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

void fullscreen_pass(WGPUCommandEncoder enc, WGPUTextureView view, WGPURenderPipeline pipe,
                     WGPUBindGroup bg, const uint32_t* dyn_off) {
    WGPURenderPassEncoder p = begin_color(enc, view, WGPUColor{0, 0, 0, 1});
    wgpuRenderPassEncoderSetPipeline(p, pipe);
    wgpuRenderPassEncoderSetBindGroup(p, 0, bg, dyn_off ? 1 : 0, dyn_off);
    wgpuRenderPassEncoderDraw(p, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(p);
    wgpuRenderPassEncoderRelease(p);
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

    WGPUBufferDescriptor bld = {};
    bld.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    bld.size = uniform_stride_ * 2;
    blur_ubo_ = wgpuDeviceCreateBuffer(device, &bld);

    build_targets();

    // Направления blur — константны для фикс. разрешения: пишем один раз (не per-frame).
    BlurUniform bh = {}; bh.dir[0] = 1.0f / bloom_w_; bh.dir[1] = 0.0f;
    BlurUniform bv = {}; bv.dir[0] = 0.0f; bv.dir[1] = 1.0f / bloom_h_;
    wgpuQueueWriteBuffer(queue, blur_ubo_, 0, &bh, sizeof(bh));
    wgpuQueueWriteBuffer(queue, blur_ubo_, uniform_stride_, &bv, sizeof(bv));

    build_gbuffer();
    build_lighting();
    build_forward();
    build_bloom();
    build_tonemap();
}

void Renderer::build_targets() {
    arena_.begin_frame();
    const WGPUTextureUsage rt = (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment |
                                                   WGPUTextureUsage_TextureBinding);
    albedo_ = arena_.acquire(TargetDesc{w_, h_, GBUFFER_ALBEDO_FMT, rt});
    normal_ = arena_.acquire(TargetDesc{w_, h_, GBUFFER_NORMAL_FMT, rt});
    hdr_ = arena_.acquire(TargetDesc{w_, h_, HDR_FMT, rt});
    bloom_w_ = w_ / 2; bloom_h_ = h_ / 2;
    bloom_a_ = arena_.acquire(TargetDesc{bloom_w_, bloom_h_, HDR_FMT, rt});
    bloom_b_ = arena_.acquire(TargetDesc{bloom_w_, bloom_h_, HDR_FMT, rt});
}

void Renderer::render(const SceneSnapshot& snap, WGPUTextureView out_view) {
    arena_.begin_frame();
    write_sprite(queue_, uniforms_, 0, snap.opaque, snap.aspect, 1.0f, 0.0f);
    write_sprite(queue_, uniforms_, uniform_stride_, snap.glow, snap.aspect, 0.62f, 2.8f);
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
    fullscreen_pass(enc, hdr_, lighting_pipe_, lighting_bg_, nullptr);

    // [transparent] forward-пасс поверх HDR (та же lighting-technique, alpha-blend).
    WGPURenderPassColorAttachment fa = {};
    fa.view = hdr_; fa.loadOp = WGPULoadOp_Load; fa.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor fpass = {};
    fpass.colorAttachmentCount = 1; fpass.colorAttachments = &fa;
    WGPURenderPassEncoder f = wgpuCommandEncoderBeginRenderPass(enc, &fpass);
    wgpuRenderPassEncoderSetPipeline(f, forward_pipe_);
    uint32_t goff = uniform_stride_;
    wgpuRenderPassEncoderSetBindGroup(f, 0, forward_bg_, 1, &goff);
    wgpuRenderPassEncoderSetVertexBuffer(f, 0, sprite_->vbo, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(f, sprite_->ibo, WGPUIndexFormat_Uint16, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(f, SPRITE_INDEX_COUNT, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(f);
    wgpuRenderPassEncoderRelease(f);

    // bloom-подграф: bright-pass → blur H → blur V (half-res, транзитные таргеты арены).
    uint32_t hoff = 0, voff = uniform_stride_;
    fullscreen_pass(enc, bloom_a_, bright_pipe_, bright_bg_, nullptr);
    fullscreen_pass(enc, bloom_b_, blur_pipe_, blur_bg_h_, &hoff);
    fullscreen_pass(enc, bloom_a_, blur_pipe_, blur_bg_v_, &voff);

    // tonemap HDR + bloom → output.
    fullscreen_pass(enc, out_view, tonemap_pipe_, tonemap_bg_, nullptr);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

void Renderer::shutdown() {
    if (tonemap_pipe_) wgpuRenderPipelineRelease(tonemap_pipe_);
    if (tonemap_bg_) wgpuBindGroupRelease(tonemap_bg_);
    if (tonemap_bgl_) wgpuBindGroupLayoutRelease(tonemap_bgl_);
    if (blur_pipe_) wgpuRenderPipelineRelease(blur_pipe_);
    if (blur_bg_v_) wgpuBindGroupRelease(blur_bg_v_);
    if (blur_bg_h_) wgpuBindGroupRelease(blur_bg_h_);
    if (blur_bgl_) wgpuBindGroupLayoutRelease(blur_bgl_);
    if (bright_pipe_) wgpuRenderPipelineRelease(bright_pipe_);
    if (bright_bg_) wgpuBindGroupRelease(bright_bg_);
    if (bright_bgl_) wgpuBindGroupLayoutRelease(bright_bgl_);
    if (forward_pipe_) wgpuRenderPipelineRelease(forward_pipe_);
    if (forward_bg_) wgpuBindGroupRelease(forward_bg_);
    if (forward_bgl_) wgpuBindGroupLayoutRelease(forward_bgl_);
    if (lighting_pipe_) wgpuRenderPipelineRelease(lighting_pipe_);
    if (lighting_bg_) wgpuBindGroupRelease(lighting_bg_);
    if (lighting_bgl_) wgpuBindGroupLayoutRelease(lighting_bgl_);
    if (gbuffer_pipe_) wgpuRenderPipelineRelease(gbuffer_pipe_);
    if (gbuffer_bg_) wgpuBindGroupRelease(gbuffer_bg_);
    if (gbuffer_bgl_) wgpuBindGroupLayoutRelease(gbuffer_bgl_);
    if (blur_ubo_) wgpuBufferRelease(blur_ubo_);
    if (lights_ubo_) wgpuBufferRelease(lights_ubo_);
    if (uniforms_) wgpuBufferRelease(uniforms_);
    arena_.shutdown();
    device_ = nullptr; queue_ = nullptr; sprite_ = nullptr;
}
