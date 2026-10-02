#include "quad_pipeline.hpp"

#include "gpu_util.hpp"
#include "quad_batch.hpp"

namespace render {
namespace {

// Флип — порядок Tiled (D, затем H, затем V) в обратную сторону: от угла выхода к углу
// источника. Тексель интерполируется в координатах текстуры и берётся `floor`: центр пикселя при
// целом масштабе попадает внутрь текселя на любом растеризаторе.
const char* kWgsl = R"(
struct Screen { size: vec2<f32>, pad: vec2<f32> };
@group(0) @binding(0) var<uniform> screen: Screen;
@group(0) @binding(1) var tex: texture_2d<f32>;

struct VIn {
    @builtin(vertex_index) vi: u32,
    @location(0) rect: vec4<f32>,
    @location(1) src: vec4<f32>,
    @location(2) rgba: u32,
    @location(3) flip: u32,
};
struct VOut {
    @builtin(position) pos: vec4<f32>,
    @location(0) texel: vec2<f32>,
    @location(1) @interpolate(flat) src: vec4<f32>,
    @location(2) @interpolate(flat) tint: vec4<f32>,
};

@vertex fn vs(in: VIn) -> VOut {
    var corners = array<vec2<f32>, 6>(vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0),
        vec2<f32>(0.0, 1.0), vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0));
    let c = corners[in.vi];
    var q = c;
    if ((in.flip & 2u) != 0u) { q.y = 1.0 - q.y; }
    if ((in.flip & 4u) != 0u) { q.x = 1.0 - q.x; }
    if ((in.flip & 1u) != 0u) { q = q.yx; }
    let p = in.rect.xy + c * in.rect.zw;
    var o: VOut;
    o.pos = vec4<f32>(p.x / screen.size.x * 2.0 - 1.0, 1.0 - p.y / screen.size.y * 2.0, 0.0, 1.0);
    o.texel = in.src.xy + q * in.src.zw;
    o.src = in.src;
    o.tint = unpack4x8unorm(in.rgba).wzyx;
    return o;
}

@fragment fn fs(in: VOut) -> @location(0) vec4<f32> {
    let hi = in.src.xy + in.src.zw - vec2<f32>(1.0, 1.0);
    let t = clamp(floor(in.texel), in.src.xy, hi);
    return textureLoad(tex, vec2<i32>(t), 0) * in.tint;
}
)";

} // namespace

WGPUBindGroupLayout make_quad_layout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry e[2] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Vertex;
    e[0].buffer.type = WGPUBufferBindingType_Uniform;
    e[0].buffer.minBindingSize = 16;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor d = {};
    d.entryCount = 2; d.entries = e;
    return wgpuDeviceCreateBindGroupLayout(device, &d);
}

WGPURenderPipeline make_quad_pipeline(WGPUDevice device, WGPUTextureFormat target,
                                      WGPUBindGroupLayout layout) {
    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &layout;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUShaderModule sm = make_shader(device, kWgsl);

    WGPUVertexAttribute a[4] = {};
    a[0].format = WGPUVertexFormat_Float32x4; a[0].offset = 0;  a[0].shaderLocation = 0;
    a[1].format = WGPUVertexFormat_Float32x4; a[1].offset = 16; a[1].shaderLocation = 1;
    a[2].format = WGPUVertexFormat_Uint32;    a[2].offset = 32; a[2].shaderLocation = 2;
    a[3].format = WGPUVertexFormat_Uint32;    a[3].offset = 36; a[3].shaderLocation = 3;
    WGPUVertexBufferLayout vbl = {};
    vbl.arrayStride = sizeof(Quad); vbl.stepMode = WGPUVertexStepMode_Instance;
    vbl.attributeCount = 4; vbl.attributes = a;

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
    rp.vertex.bufferCount = 1; rp.vertex.buffers = &vbl;
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(device, &rp);

    wgpuShaderModuleRelease(sm);
    wgpuPipelineLayoutRelease(pl);
    return pipe;
}

WGPUTextureFormat quad_texel_format(WGPUTextureFormat target) {
    switch (target) {
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8UnormSrgb: return WGPUTextureFormat_RGBA8UnormSrgb;
    default:                               return WGPUTextureFormat_RGBA8Unorm;
    }
}

} // namespace render
