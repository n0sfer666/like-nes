#include "renderer.hpp"

#include "gpu_util.hpp"
#include "renderer_internal.hpp"
#include "shaders.hpp"

#include <string>

void Renderer::build_forward() {
    WGPUBindGroupLayoutEntry e[5] = {};
    e[0].binding = 0;
    e[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    e[0].buffer.type = WGPUBufferBindingType_Uniform;
    e[0].buffer.hasDynamicOffset = true;
    e[0].buffer.minBindingSize = sizeof(SpriteUniform);
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].sampler.type = WGPUSamplerBindingType_Filtering;
    e[2].binding = 2; e[2].visibility = WGPUShaderStage_Fragment;
    e[2].texture.sampleType = WGPUTextureSampleType_Float;
    e[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[3] = e[2]; e[3].binding = 3;
    e[4].binding = 4; e[4].visibility = WGPUShaderStage_Fragment;
    e[4].buffer.type = WGPUBufferBindingType_Uniform;
    e[4].buffer.minBindingSize = sizeof(LightsUniform);
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 5; bgld.entries = e;
    forward_bgl_ = wgpuDeviceCreateBindGroupLayout(device_, &bgld);

    WGPUBindGroupEntry b[5] = {};
    b[0].binding = 0; b[0].buffer = uniforms_; b[0].size = sizeof(SpriteUniform);
    b[1].binding = 1; b[1].sampler = sprite_->sampler;
    b[2].binding = 2; b[2].textureView = sprite_->albedo;
    b[3].binding = 3; b[3].textureView = sprite_->normal;
    b[4].binding = 4; b[4].buffer = lights_ubo_; b[4].size = sizeof(LightsUniform);
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = forward_bgl_; bgd.entryCount = 5; bgd.entries = b;
    forward_bg_ = wgpuDeviceCreateBindGroup(device_, &bgd);

    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &forward_bgl_;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device_, &pld);

    std::string src = forward_wgsl();
    WGPUShaderModule sm = make_shader(device_, src.c_str());

    WGPUVertexAttribute attr[2] = {};
    attr[0].format = WGPUVertexFormat_Float32x2; attr[0].offset = 0; attr[0].shaderLocation = 0;
    attr[1].format = WGPUVertexFormat_Float32x2; attr[1].offset = 8; attr[1].shaderLocation = 1;
    WGPUVertexBufferLayout vbl = {};
    vbl.arrayStride = 16; vbl.stepMode = WGPUVertexStepMode_Vertex;
    vbl.attributeCount = 2; vbl.attributes = attr;

    WGPUBlendState blend = {};
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.color.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_One;
    blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.operation = WGPUBlendOperation_Add;
    WGPUColorTargetState target = color_target(HDR_FMT);
    target.blend = &blend;

    WGPUFragmentState fs = {};
    fs.module = sm; fs.entryPoint = "fs"; fs.targetCount = 1; fs.targets = &target;

    WGPURenderPipelineDescriptor rp = {};
    rp.layout = pl;
    rp.vertex.module = sm; rp.vertex.entryPoint = "vs";
    rp.vertex.bufferCount = 1; rp.vertex.buffers = &vbl;
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    forward_pipe_ = wgpuDeviceCreateRenderPipeline(device_, &rp);

    wgpuShaderModuleRelease(sm);
    wgpuPipelineLayoutRelease(pl);
}
