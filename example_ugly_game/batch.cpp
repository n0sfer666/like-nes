#include "batch.hpp"
#include "world.hpp"

namespace game {
namespace {

const char* kWgsl = R"(
struct VP { half_extent: vec2<f32>, pad: vec2<f32> };
@group(0) @binding(0) var<uniform> vp: VP;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var tex: texture_2d<f32>;

struct VIn {
    @location(0) qpos: vec2<f32>,
    @location(1) quv: vec2<f32>,
    @location(2) ipos: vec2<f32>,
    @location(3) isize: vec2<f32>,
    @location(4) iuv0: vec2<f32>,
    @location(5) iuv1: vec2<f32>,
    @location(6) itint: vec4<f32>,
    @location(7) irot: f32,
};
struct VOut {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) tint: vec4<f32>,
};

@vertex fn vs(in: VIn) -> VOut {
    let c = cos(in.irot);
    let s = sin(in.irot);
    let rq = vec2<f32>(in.qpos.x * c - in.qpos.y * s, in.qpos.x * s + in.qpos.y * c);
    let world = in.ipos + rq * in.isize;
    let ndc = world / vp.half_extent;
    var o: VOut;
    o.pos = vec4<f32>(ndc.x, ndc.y, 0.0, 1.0);
    o.uv = mix(in.iuv0, in.iuv1, in.quv);
    o.tint = in.itint;
    return o;
}
@fragment fn fs(in: VOut) -> @location(0) vec4<f32> {
    return textureSample(tex, samp, in.uv) * in.tint;
}
)";

WGPUShaderModule make_shader(WGPUDevice device, const char* src) {
    WGPUShaderModuleWGSLDescriptor wgsl = {};
    wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl.code = src;
    WGPUShaderModuleDescriptor sd = {};
    sd.nextInChain = &wgsl.chain;
    return wgpuDeviceCreateShaderModule(device, &sd);
}

WGPUBuffer make_buffer(WGPUDevice device, WGPUQueue queue, WGPUBufferUsage usage,
                       const void* data, size_t size) {
    WGPUBufferDescriptor bd = {};
    bd.usage = usage | WGPUBufferUsage_CopyDst;
    bd.size = size;
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &bd);
    if (data) wgpuQueueWriteBuffer(queue, buf, 0, data, size);
    return buf;
}

} // namespace

void SpriteBatch::init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target,
                       const Atlas& atlas) {
    device_ = device; queue_ = queue;
    cpu_ = std::make_unique<Instance[]>(MAX_INSTANCES);
    stage_ = InstanceStage(cpu_.get(), MAX_INSTANCES);

    // Baked-путь (шов assetc→билд): BC7 из бандла. Иначе RGBA (процедурный/mobile-шелл).
    const bool baked = !atlas.bc7.empty();
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{atlas.w, atlas.h, 1};
    td.format = baked ? WGPUTextureFormat_BC7RGBAUnorm : WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1; td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    tex_ = wgpuDeviceCreateTexture(device, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex_; dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    const uint8_t* src; size_t src_sz;
    if (baked) {
        layout.bytesPerRow = ((atlas.w + 3) / 4) * 16; // BC7: 16 байт/4x4-блок
        layout.rowsPerImage = (atlas.h + 3) / 4;
        src = atlas.bc7.data(); src_sz = atlas.bc7.size();
    } else {
        layout.bytesPerRow = 4 * atlas.w; layout.rowsPerImage = atlas.h;
        src = atlas.px.data(); src_sz = atlas.px.size();
    }
    WGPUExtent3D ext = {atlas.w, atlas.h, 1};
    wgpuQueueWriteTexture(queue, &dst, src, src_sz, &layout, &ext);
    view_ = wgpuTextureCreateView(tex_, nullptr);

    WGPUSamplerDescriptor sd = {};
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    sd.magFilter = WGPUFilterMode_Nearest;
    sd.minFilter = WGPUFilterMode_Nearest;
    sd.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sd.lodMaxClamp = 1.0f; sd.maxAnisotropy = 1;
    sampler_ = wgpuDeviceCreateSampler(device, &sd);

    const float verts[] = {
        -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f,
    };
    const uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    quad_vbo_ = make_buffer(device, queue, WGPUBufferUsage_Vertex, verts, sizeof(verts));
    quad_ibo_ = make_buffer(device, queue, WGPUBufferUsage_Index, idx, sizeof(idx));
    inst_vbo_ = make_buffer(device, queue, WGPUBufferUsage_Vertex, nullptr,
                            sizeof(Instance) * MAX_INSTANCES);

    vp_ubo_ = make_buffer(device, queue, WGPUBufferUsage_Uniform, nullptr, 16);

    WGPUBindGroupLayoutEntry e[3] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Vertex;
    e[0].buffer.type = WGPUBufferBindingType_Uniform;
    e[0].buffer.minBindingSize = 16;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].sampler.type = WGPUSamplerBindingType_Filtering;
    e[2].binding = 2; e[2].visibility = WGPUShaderStage_Fragment;
    e[2].texture.sampleType = WGPUTextureSampleType_Float;
    e[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 3; bgld.entries = e;
    bgl_ = wgpuDeviceCreateBindGroupLayout(device, &bgld);

    WGPUBindGroupEntry b[3] = {};
    b[0].binding = 0; b[0].buffer = vp_ubo_; b[0].size = 16;
    b[1].binding = 1; b[1].sampler = sampler_;
    b[2].binding = 2; b[2].textureView = view_;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = bgl_; bgd.entryCount = 3; bgd.entries = b;
    bg_ = wgpuDeviceCreateBindGroup(device, &bgd);

    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl_;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUShaderModule sm = make_shader(device, kWgsl);

    WGPUVertexAttribute qa[2] = {};
    qa[0].format = WGPUVertexFormat_Float32x2; qa[0].offset = 0; qa[0].shaderLocation = 0;
    qa[1].format = WGPUVertexFormat_Float32x2; qa[1].offset = 8; qa[1].shaderLocation = 1;
    WGPUVertexAttribute ia[6] = {};
    ia[0].format = WGPUVertexFormat_Float32x2; ia[0].offset = 0;  ia[0].shaderLocation = 2;
    ia[1].format = WGPUVertexFormat_Float32x2; ia[1].offset = 8;  ia[1].shaderLocation = 3;
    ia[2].format = WGPUVertexFormat_Float32x2; ia[2].offset = 16; ia[2].shaderLocation = 4;
    ia[3].format = WGPUVertexFormat_Float32x2; ia[3].offset = 24; ia[3].shaderLocation = 5;
    ia[4].format = WGPUVertexFormat_Float32x4; ia[4].offset = 32; ia[4].shaderLocation = 6;
    ia[5].format = WGPUVertexFormat_Float32;   ia[5].offset = 48; ia[5].shaderLocation = 7;
    WGPUVertexBufferLayout vbl[2] = {};
    vbl[0].arrayStride = 16; vbl[0].stepMode = WGPUVertexStepMode_Vertex;
    vbl[0].attributeCount = 2; vbl[0].attributes = qa;
    vbl[1].arrayStride = sizeof(Instance); vbl[1].stepMode = WGPUVertexStepMode_Instance;
    vbl[1].attributeCount = 6; vbl[1].attributes = ia;

    WGPUBlendState blend = {};
    blend.color.operation = WGPUBlendOperation_Add;
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_One;
    blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    WGPUColorTargetState ct = {};
    ct.format = target; ct.blend = &blend; ct.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fs = {};
    fs.module = sm; fs.entryPoint = "fs"; fs.targetCount = 1; fs.targets = &ct;

    WGPURenderPipelineDescriptor rp = {};
    rp.layout = pl;
    rp.vertex.module = sm; rp.vertex.entryPoint = "vs";
    rp.vertex.bufferCount = 2; rp.vertex.buffers = vbl;
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    pipe_ = wgpuDeviceCreateRenderPipeline(device, &rp);

    wgpuShaderModuleRelease(sm);
    wgpuPipelineLayoutRelease(pl);
    set_viewport(VIEW_W, VIEW_H);
}

void SpriteBatch::set_viewport(uint32_t w, uint32_t h) {
    const float vp[4] = {(float)w * 0.5f, (float)h * 0.5f, 0, 0};
    wgpuQueueWriteBuffer(queue_, vp_ubo_, 0, vp, sizeof(vp));
}

void SpriteBatch::begin() { stage_.begin(); }

void SpriteBatch::push(const Instance& inst) { stage_.push(inst); }

void SpriteBatch::flush(WGPURenderPassEncoder pass) {
    if (stage_.count() == 0) return;
    wgpuQueueWriteBuffer(queue_, inst_vbo_, 0, stage_.data(), stage_.bytes());
    wgpuRenderPassEncoderSetPipeline(pass, pipe_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bg_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, quad_vbo_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, inst_vbo_, 0, stage_.bytes());
    wgpuRenderPassEncoderSetIndexBuffer(pass, quad_ibo_, WGPUIndexFormat_Uint16, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, 6, stage_.count(), 0, 0, 0);
}

void SpriteBatch::shutdown() {
    if (pipe_) wgpuRenderPipelineRelease(pipe_);
    if (bg_) wgpuBindGroupRelease(bg_);
    if (bgl_) wgpuBindGroupLayoutRelease(bgl_);
    if (vp_ubo_) wgpuBufferRelease(vp_ubo_);
    if (inst_vbo_) wgpuBufferRelease(inst_vbo_);
    if (quad_ibo_) wgpuBufferRelease(quad_ibo_);
    if (quad_vbo_) wgpuBufferRelease(quad_vbo_);
    if (sampler_) wgpuSamplerRelease(sampler_);
    if (view_) wgpuTextureViewRelease(view_);
    if (tex_) wgpuTextureRelease(tex_);
}

WGPURenderPassEncoder begin_clear(WGPUCommandEncoder enc, WGPUTextureView view, WGPUColor clear) {
    WGPURenderPassColorAttachment a = {};
    a.view = view; a.loadOp = WGPULoadOp_Clear; a.storeOp = WGPUStoreOp_Store;
    a.clearValue = clear;
    WGPURenderPassDescriptor d = {};
    d.colorAttachmentCount = 1; d.colorAttachments = &a;
    return wgpuCommandEncoderBeginRenderPass(enc, &d);
}

} // namespace game
