#include "renderer.hpp"

#include "gpu_util.hpp"
#include "renderer_internal.hpp"
#include "shaders.hpp"

void Renderer::build_gbuffer() {
    WGPUBindGroupLayoutEntry e[4] = {};
    e[0].binding = 0;
    e[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    e[0].buffer.type = WGPUBufferBindingType_Uniform;
    e[0].buffer.hasDynamicOffset = true;
    e[0].buffer.minBindingSize = sizeof(SpriteUniform);
    e[1].binding = 1;
    e[1].visibility = WGPUShaderStage_Fragment;
    e[1].sampler.type = WGPUSamplerBindingType_Filtering;
    e[2].binding = 2;
    e[2].visibility = WGPUShaderStage_Fragment;
    e[2].texture.sampleType = WGPUTextureSampleType_Float;
    e[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[3] = e[2];
    e[3].binding = 3;
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 4;
    bgld.entries = e;
    gbuffer_bgl_ = wgpuDeviceCreateBindGroupLayout(device_, &bgld);

    WGPUBindGroupEntry b[4] = {};
    b[0].binding = 0; b[0].buffer = uniforms_; b[0].offset = 0; b[0].size = sizeof(SpriteUniform);
    b[1].binding = 1; b[1].sampler = sprite_->sampler;
    b[2].binding = 2; b[2].textureView = sprite_->albedo;
    b[3].binding = 3; b[3].textureView = sprite_->normal;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = gbuffer_bgl_;
    bgd.entryCount = 4;
    bgd.entries = b;
    gbuffer_bg_ = wgpuDeviceCreateBindGroup(device_, &bgd);

    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = &gbuffer_bgl_;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device_, &pld);

    WGPUShaderModule sm = make_shader(device_, gbuffer_wgsl());

    WGPUVertexAttribute attr[2] = {};
    attr[0].format = WGPUVertexFormat_Float32x2; attr[0].offset = 0; attr[0].shaderLocation = 0;
    attr[1].format = WGPUVertexFormat_Float32x2; attr[1].offset = 8; attr[1].shaderLocation = 1;
    WGPUVertexBufferLayout vbl = {};
    vbl.arrayStride = 16;
    vbl.stepMode = WGPUVertexStepMode_Vertex;
    vbl.attributeCount = 2;
    vbl.attributes = attr;

    WGPUColorTargetState targets[2] = {color_target(GBUFFER_ALBEDO_FMT),
                                       color_target(GBUFFER_NORMAL_FMT)};
    WGPUFragmentState fs = {};
    fs.module = sm; fs.entryPoint = "fs";
    fs.targetCount = 2; fs.targets = targets;

    WGPURenderPipelineDescriptor rp = {};
    rp.layout = pl;
    rp.vertex.module = sm; rp.vertex.entryPoint = "vs";
    rp.vertex.bufferCount = 1; rp.vertex.buffers = &vbl;
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    gbuffer_pipe_ = wgpuDeviceCreateRenderPipeline(device_, &rp);

    wgpuShaderModuleRelease(sm);
    wgpuPipelineLayoutRelease(pl);
}

void Renderer::build_preview() {
    WGPUBindGroupLayoutEntry e[2] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Fragment;
    e[0].sampler.type = WGPUSamplerBindingType_Filtering;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 2; bgld.entries = e;
    preview_bgl_ = wgpuDeviceCreateBindGroupLayout(device_, &bgld);

    WGPUBindGroupEntry b[2] = {};
    b[0].binding = 0; b[0].sampler = sprite_->sampler;
    b[1].binding = 1; b[1].textureView = normal_;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = preview_bgl_; bgd.entryCount = 2; bgd.entries = b;
    preview_bg_ = wgpuDeviceCreateBindGroup(device_, &bgd);

    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &preview_bgl_;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device_, &pld);

    WGPUShaderModule vsm = make_shader(device_, fullscreen_vs_wgsl());
    WGPUShaderModule fsm = make_shader(device_, preview_fs_wgsl());
    WGPUColorTargetState target = color_target(out_format_);
    WGPUFragmentState fs = {};
    fs.module = fsm; fs.entryPoint = "fs"; fs.targetCount = 1; fs.targets = &target;

    WGPURenderPipelineDescriptor rp = {};
    rp.layout = pl;
    rp.vertex.module = vsm; rp.vertex.entryPoint = "vs";
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    preview_pipe_ = wgpuDeviceCreateRenderPipeline(device_, &rp);

    wgpuShaderModuleRelease(vsm);
    wgpuShaderModuleRelease(fsm);
    wgpuPipelineLayoutRelease(pl);
}
