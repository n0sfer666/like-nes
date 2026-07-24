#include "bloom.hpp"
#include "bloom_shaders.hpp"

#include <string>

namespace game {
namespace {

constexpr WGPUTextureFormat HDR = WGPUTextureFormat_RGBA16Float;

WGPUShaderModule shader(WGPUDevice d, const char* vs, const char* fs) {
    std::string src = std::string(vs) + fs;
    WGPUShaderModuleWGSLDescriptor w = {};
    w.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    w.code = src.c_str();
    WGPUShaderModuleDescriptor md = {}; md.nextInChain = &w.chain;
    return wgpuDeviceCreateShaderModule(d, &md);
}

void mk_tex(WGPUDevice d, uint32_t w, uint32_t h, WGPUTexture* t, WGPUTextureView* v) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D; td.size = {w, h, 1}; td.format = HDR;
    td.mipLevelCount = 1; td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    *t = wgpuDeviceCreateTexture(d, &td);
    *v = wgpuTextureCreateView(*t, nullptr);
}

WGPURenderPipeline fs_pipe(WGPUDevice d, WGPUBindGroupLayout bgl, WGPUShaderModule sm,
                           WGPUTextureFormat fmt) {
    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(d, &pld);
    WGPUColorTargetState ct = {}; ct.format = fmt; ct.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fs = {}; fs.module = sm; fs.entryPoint = "fs"; fs.targetCount = 1; fs.targets = &ct;
    WGPURenderPipelineDescriptor rp = {};
    rp.layout = pl; rp.vertex.module = sm; rp.vertex.entryPoint = "vs";
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF; rp.fragment = &fs;
    WGPURenderPipeline p = wgpuDeviceCreateRenderPipeline(d, &rp);
    wgpuPipelineLayoutRelease(pl);
    return p;
}

WGPUBindGroupLayout bgl_st(WGPUDevice d, bool with_ubo, bool two_tex) {
    WGPUBindGroupLayoutEntry e[4] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Fragment;
    e[0].sampler.type = WGPUSamplerBindingType_Filtering;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    uint32_t n = 2;
    if (two_tex) {
        e[2].binding = 2; e[2].visibility = WGPUShaderStage_Fragment;
        e[2].texture.sampleType = WGPUTextureSampleType_Float;
        e[2].texture.viewDimension = WGPUTextureViewDimension_2D; n = 3;
    } else if (with_ubo) {
        e[2].binding = 2; e[2].visibility = WGPUShaderStage_Fragment;
        e[2].buffer.type = WGPUBufferBindingType_Uniform; e[2].buffer.minBindingSize = 16; n = 3;
    }
    WGPUBindGroupLayoutDescriptor bd = {}; bd.entryCount = n; bd.entries = e;
    return wgpuDeviceCreateBindGroupLayout(d, &bd);
}

WGPUBuffer ubo16(WGPUDevice d, WGPUQueue q, float dx, float dy) {
    float u[4] = {dx, dy, 0, 0};
    WGPUBufferDescriptor bd = {};
    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst; bd.size = 16;
    WGPUBuffer b = wgpuDeviceCreateBuffer(d, &bd);
    wgpuQueueWriteBuffer(q, b, 0, u, 16);
    return b;
}

WGPUBindGroup bg(WGPUDevice d, WGPUBindGroupLayout l, WGPUSampler s, WGPUTextureView t0,
                 WGPUTextureView t1, WGPUBuffer u) {
    WGPUBindGroupEntry e[3] = {};
    e[0].binding = 0; e[0].sampler = s;
    e[1].binding = 1; e[1].textureView = t0;
    uint32_t n = 2;
    if (t1) { e[2].binding = 2; e[2].textureView = t1; n = 3; }
    else if (u) { e[2].binding = 2; e[2].buffer = u; e[2].size = 16; n = 3; }
    WGPUBindGroupDescriptor bd = {}; bd.layout = l; bd.entryCount = n; bd.entries = e;
    return wgpuDeviceCreateBindGroup(d, &bd);
}

void pass(WGPUCommandEncoder enc, WGPUTextureView target, WGPURenderPipeline pipe, WGPUBindGroup b) {
    WGPURenderPassColorAttachment a = {};
    a.view = target; a.loadOp = WGPULoadOp_Clear; a.storeOp = WGPUStoreOp_Store;
    a.clearValue = {0, 0, 0, 1};
    WGPURenderPassDescriptor rp = {}; rp.colorAttachmentCount = 1; rp.colorAttachments = &a;
    WGPURenderPassEncoder p = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    wgpuRenderPassEncoderSetPipeline(p, pipe);
    wgpuRenderPassEncoderSetBindGroup(p, 0, b, 0, nullptr);
    wgpuRenderPassEncoderDraw(p, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(p);
    wgpuRenderPassEncoderRelease(p);
}

} // namespace

bool Bloom::init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat out_fmt, uint32_t w, uint32_t h) {
    const uint32_t bw = w / 2, bh = h / 2;
    mk_tex(device, w, h, &hdr_, &hdr_view_);
    mk_tex(device, bw, bh, &bloom_a_, &bloom_a_v_);
    mk_tex(device, bw, bh, &bloom_b_, &bloom_b_v_);

    WGPUSamplerDescriptor sd = {};
    sd.magFilter = WGPUFilterMode_Linear; sd.minFilter = WGPUFilterMode_Linear;
    sd.addressModeU = WGPUAddressMode_ClampToEdge; sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge; sd.maxAnisotropy = 1;
    samp_ = wgpuDeviceCreateSampler(device, &sd);
    ubo_h_ = ubo16(device, queue, 1.0f / bw, 0);
    ubo_v_ = ubo16(device, queue, 0, 1.0f / bh);

    bright_bgl_ = bgl_st(device, false, false);
    blur_bgl_ = bgl_st(device, true, false);
    tone_bgl_ = bgl_st(device, false, true);
    WGPUShaderModule bs = shader(device, BLOOM_VS, BLOOM_BRIGHT_FS);
    WGPUShaderModule ls = shader(device, BLOOM_VS, BLOOM_BLUR_FS);
    WGPUShaderModule ts = shader(device, BLOOM_VS, BLOOM_TONE_FS);
    bright_pipe_ = fs_pipe(device, bright_bgl_, bs, HDR);
    blur_pipe_ = fs_pipe(device, blur_bgl_, ls, HDR);
    tone_pipe_ = fs_pipe(device, tone_bgl_, ts, out_fmt);
    wgpuShaderModuleRelease(bs); wgpuShaderModuleRelease(ls); wgpuShaderModuleRelease(ts);

    bright_bg_ = bg(device, bright_bgl_, samp_, hdr_view_, nullptr, nullptr);
    blur_bg_h_ = bg(device, blur_bgl_, samp_, bloom_a_v_, nullptr, ubo_h_);
    blur_bg_v_ = bg(device, blur_bgl_, samp_, bloom_b_v_, nullptr, ubo_v_);
    tone_bg_ = bg(device, tone_bgl_, samp_, hdr_view_, bloom_a_v_, nullptr);
    return bright_pipe_ && blur_pipe_ && tone_pipe_;
}

void Bloom::resolve(WGPUCommandEncoder enc, WGPUTextureView out_view) {
    pass(enc, bloom_a_v_, bright_pipe_, bright_bg_);   // HDR → bloom_a (bright)
    pass(enc, bloom_b_v_, blur_pipe_, blur_bg_h_);     // a → b (horizontal)
    pass(enc, bloom_a_v_, blur_pipe_, blur_bg_v_);     // b → a (vertical)
    pass(enc, out_view, tone_pipe_, tone_bg_);         // HDR + bloom_a → swapchain
}

void Bloom::shutdown() {
    if (bright_pipe_) wgpuRenderPipelineRelease(bright_pipe_);
    if (blur_pipe_) wgpuRenderPipelineRelease(blur_pipe_);
    if (tone_pipe_) wgpuRenderPipelineRelease(tone_pipe_);
    if (bright_bg_) wgpuBindGroupRelease(bright_bg_);
    if (blur_bg_h_) wgpuBindGroupRelease(blur_bg_h_);
    if (blur_bg_v_) wgpuBindGroupRelease(blur_bg_v_);
    if (tone_bg_) wgpuBindGroupRelease(tone_bg_);
    if (bright_bgl_) wgpuBindGroupLayoutRelease(bright_bgl_);
    if (blur_bgl_) wgpuBindGroupLayoutRelease(blur_bgl_);
    if (tone_bgl_) wgpuBindGroupLayoutRelease(tone_bgl_);
    if (ubo_h_) wgpuBufferRelease(ubo_h_);
    if (ubo_v_) wgpuBufferRelease(ubo_v_);
    if (samp_) wgpuSamplerRelease(samp_);
    if (hdr_view_) wgpuTextureViewRelease(hdr_view_);
    if (bloom_a_v_) wgpuTextureViewRelease(bloom_a_v_);
    if (bloom_b_v_) wgpuTextureViewRelease(bloom_b_v_);
    if (hdr_) wgpuTextureRelease(hdr_);
    if (bloom_a_) wgpuTextureRelease(bloom_a_);
    if (bloom_b_) wgpuTextureRelease(bloom_b_);
}

} // namespace game
