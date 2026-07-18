#include "renderer.hpp"

#include "gpu_util.hpp"
#include "renderer_internal.hpp"
#include "shaders.hpp"

namespace {

WGPUBindGroupLayout sampler_tex_bgl(WGPUDevice device, bool with_uniform, size_t ubo_size) {
    WGPUBindGroupLayoutEntry e[3] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Fragment;
    e[0].sampler.type = WGPUSamplerBindingType_Filtering;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[2].binding = 2; e[2].visibility = WGPUShaderStage_Fragment;
    e[2].buffer.type = WGPUBufferBindingType_Uniform;
    e[2].buffer.hasDynamicOffset = true;
    e[2].buffer.minBindingSize = ubo_size;
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = with_uniform ? 3 : 2; bgld.entries = e;
    return wgpuDeviceCreateBindGroupLayout(device, &bgld);
}

} // namespace

void Renderer::build_bloom() {
    // bright-pass: HDR → bloom_a_ (half-res).
    bright_bgl_ = sampler_tex_bgl(device_, false, 0);
    WGPUBindGroupEntry bb[2] = {};
    bb[0].binding = 0; bb[0].sampler = sprite_->sampler;
    bb[1].binding = 1; bb[1].textureView = hdr_;
    WGPUBindGroupDescriptor bbd = {};
    bbd.layout = bright_bgl_; bbd.entryCount = 2; bbd.entries = bb;
    bright_bg_ = wgpuDeviceCreateBindGroup(device_, &bbd);
    WGPUShaderModule bsm = make_shader(device_, brightpass_fs_wgsl());
    bright_pipe_ = make_fullscreen_pipe(device_, bright_bgl_, bsm, HDR_FMT);
    wgpuShaderModuleRelease(bsm);

    // separable blur ping-pong: h (bloom_a_→bloom_b_, off 0) + v (bloom_b_→bloom_a_, off 256).
    blur_bgl_ = sampler_tex_bgl(device_, true, sizeof(BlurUniform));
    WGPUBindGroupEntry hb[3] = {};
    hb[0].binding = 0; hb[0].sampler = sprite_->sampler;
    hb[1].binding = 1; hb[1].textureView = bloom_a_;
    hb[2].binding = 2; hb[2].buffer = blur_ubo_; hb[2].size = sizeof(BlurUniform);
    WGPUBindGroupDescriptor hbd = {};
    hbd.layout = blur_bgl_; hbd.entryCount = 3; hbd.entries = hb;
    blur_bg_h_ = wgpuDeviceCreateBindGroup(device_, &hbd);

    WGPUBindGroupEntry vb[3] = {};
    vb[0].binding = 0; vb[0].sampler = sprite_->sampler;
    vb[1].binding = 1; vb[1].textureView = bloom_b_;
    vb[2].binding = 2; vb[2].buffer = blur_ubo_; vb[2].size = sizeof(BlurUniform);
    WGPUBindGroupDescriptor vbd = {};
    vbd.layout = blur_bgl_; vbd.entryCount = 3; vbd.entries = vb;
    blur_bg_v_ = wgpuDeviceCreateBindGroup(device_, &vbd);

    WGPUShaderModule lsm = make_shader(device_, blur_fs_wgsl());
    blur_pipe_ = make_fullscreen_pipe(device_, blur_bgl_, lsm, HDR_FMT);
    wgpuShaderModuleRelease(lsm);
}
