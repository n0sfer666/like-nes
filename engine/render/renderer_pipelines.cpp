#include "renderer.hpp"

#include "gpu_util.hpp"
#include "renderer_internal.hpp"
#include "shaders.hpp"

#include <string>

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

// Fullscreen-пайплайн (VS = fullscreen-triangle) с готовыми bind-group-layout и fragment-модулем.
WGPURenderPipeline make_fullscreen_pipe(WGPUDevice device, WGPUBindGroupLayout bgl,
                                        WGPUShaderModule fsm, WGPUTextureFormat fmt) {
    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);

    WGPUShaderModule vsm = make_shader(device, fullscreen_vs_wgsl());
    WGPUColorTargetState target = color_target(fmt);
    WGPUFragmentState fs = {};
    fs.module = fsm; fs.entryPoint = "fs"; fs.targetCount = 1; fs.targets = &target;

    WGPURenderPipelineDescriptor rp = {};
    rp.layout = pl;
    rp.vertex.module = vsm; rp.vertex.entryPoint = "vs";
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(device, &rp);

    wgpuShaderModuleRelease(vsm);
    wgpuPipelineLayoutRelease(pl);
    return pipe;
}

void Renderer::build_lighting() {
    WGPUBindGroupLayoutEntry e[4] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Fragment;
    e[0].sampler.type = WGPUSamplerBindingType_Filtering;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[2] = e[1]; e[2].binding = 2;
    e[3].binding = 3; e[3].visibility = WGPUShaderStage_Fragment;
    e[3].buffer.type = WGPUBufferBindingType_Uniform;
    e[3].buffer.minBindingSize = sizeof(LightsUniform);
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 4; bgld.entries = e;
    lighting_bgl_ = wgpuDeviceCreateBindGroupLayout(device_, &bgld);

    WGPUBindGroupEntry b[4] = {};
    b[0].binding = 0; b[0].sampler = sprite_->sampler;
    b[1].binding = 1; b[1].textureView = albedo_;
    b[2].binding = 2; b[2].textureView = normal_;
    b[3].binding = 3; b[3].buffer = lights_ubo_; b[3].size = sizeof(LightsUniform);
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = lighting_bgl_; bgd.entryCount = 4; bgd.entries = b;
    lighting_bg_ = wgpuDeviceCreateBindGroup(device_, &bgd);

    std::string src = deferred_lighting_wgsl();
    WGPUShaderModule fsm = make_shader(device_, src.c_str());
    lighting_pipe_ = make_fullscreen_pipe(device_, lighting_bgl_, fsm, HDR_FMT);
    wgpuShaderModuleRelease(fsm);
}

void Renderer::build_tonemap() {
    WGPUBindGroupLayoutEntry e[3] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Fragment;
    e[0].sampler.type = WGPUSamplerBindingType_Filtering;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[2] = e[1]; e[2].binding = 2;
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 3; bgld.entries = e;
    tonemap_bgl_ = wgpuDeviceCreateBindGroupLayout(device_, &bgld);

    WGPUBindGroupEntry b[3] = {};
    b[0].binding = 0; b[0].sampler = sprite_->sampler;
    b[1].binding = 1; b[1].textureView = hdr_;
    b[2].binding = 2; b[2].textureView = bloom_a_;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = tonemap_bgl_; bgd.entryCount = 3; bgd.entries = b;
    tonemap_bg_ = wgpuDeviceCreateBindGroup(device_, &bgd);

    WGPUShaderModule fsm = make_shader(device_, tonemap_fs_wgsl());
    tonemap_pipe_ = make_fullscreen_pipe(device_, tonemap_bgl_, fsm, out_format_);
    wgpuShaderModuleRelease(fsm);
}
