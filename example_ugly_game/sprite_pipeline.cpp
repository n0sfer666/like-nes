#include "sprite_pipeline.hpp"

#include "instance.hpp"

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

} // namespace

WGPURenderPipeline make_sprite_pipeline(WGPUDevice device, WGPUTextureFormat target,
                                        WGPUBindGroupLayout bgl) {
    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
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
    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(device, &rp);

    wgpuShaderModuleRelease(sm);
    wgpuPipelineLayoutRelease(pl);
    return pipe;
}

} // namespace game
