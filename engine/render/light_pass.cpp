#include "light_pass.hpp"

#include <cstring>
#include <vector>

#include "gpu_util.hpp"
#include "renderer_internal.hpp"
#include "shaders_light.hpp"

namespace lightgfx {
namespace {

// Плоская нормаль (0,0,1) в кодировке текстуры: нормали спрайтов приходят шагом B, а до него
// проход обязан работать и без них — иначе «свет как данные» нечем проверить кадром.
constexpr uint8_t FLAT_NORMAL[4] = {128, 128, 255, 255};
// «Ничего не перекрывает» в кодировке буфера окклюзии: то же значение, что и его цвет очистки.
// Проход, которому буфер не дали, обязан светить ровно так, как светил до появления теней.
constexpr uint8_t OPEN_OCC[4] = {0, 0, 0, 255};

WGPUTexture make_pixel(WGPUDevice device, WGPUQueue queue, const uint8_t rgba[4]) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{1, 1, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4;
    layout.rowsPerImage = 1;
    WGPUExtent3D ext = {1, 1, 1};
    wgpuQueueWriteTexture(queue, &dst, rgba, 4, &layout, &ext);
    return tex;
}

void fill(GpuLight& g, const light::LightRow& r) {
    g.pos_h[0] = r.pos[0]; g.pos_h[1] = r.pos[1]; g.pos_h[2] = r.height; g.pos_h[3] = r.radius;
    g.color_i[0] = r.color[0]; g.color_i[1] = r.color[1]; g.color_i[2] = r.color[2];
    g.color_i[3] = r.intensity;
    g.dir_k[0] = r.dir[0]; g.dir_k[1] = r.dir[1];
    g.dir_k[2] = r.kind == static_cast<uint8_t>(light::Kind::Directional) ? 1.0f : 0.0f;
    g.dir_k[3] = r.shadow;
}

} // namespace

bool Pass::init(WGPUDevice device, WGPUQueue queue, const light::Table& table,
                WGPUTextureFormat fmt, float aspect) {
    device_ = device;
    lights_ = table.count();
    if (lights_ == 0 || table.ambient() == nullptr) return false;

    std::vector<GpuLight> gpu(lights_);
    for (uint32_t i = 0; i < lights_; ++i) {
        const light::LightRow* r = table.row(i);
        if (r == nullptr) return false;
        fill(gpu[i], *r);
    }
    WGPUBufferDescriptor bd = {};
    bd.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
    bd.size = sizeof(GpuLight) * gpu.size();
    lights_ssbo_ = wgpuDeviceCreateBuffer(device, &bd);
    wgpuQueueWriteBuffer(queue, lights_ssbo_, 0, gpu.data(), static_cast<size_t>(bd.size));

    FrameUniform fu = {};
    std::memcpy(fu.ambient, table.ambient(), sizeof(fu.ambient));
    fu.view[0] = aspect;
    WGPUBufferDescriptor ud = {};
    ud.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    ud.size = sizeof(FrameUniform);
    frame_ubo_ = wgpuDeviceCreateBuffer(device, &ud);
    wgpuQueueWriteBuffer(queue, frame_ubo_, 0, &fu, sizeof(fu));

    WGPUSamplerDescriptor sd = {};
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sd.lodMaxClamp = 1.0f;
    sd.maxAnisotropy = 1;
    sampler_ = wgpuDeviceCreateSampler(device, &sd);

    flat_normal_ = make_pixel(device, queue, FLAT_NORMAL);
    open_occ_ = make_pixel(device, queue, OPEN_OCC);
    flat_normal_view_ = wgpuTextureCreateView(flat_normal_, nullptr);
    open_occ_view_ = wgpuTextureCreateView(open_occ_, nullptr);

    WGPUBindGroupLayoutEntry e[6] = {};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Fragment;
    e[0].sampler.type = WGPUSamplerBindingType_Filtering;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment;
    e[1].texture.sampleType = WGPUTextureSampleType_Float;
    e[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[2] = e[1]; e[2].binding = 2;
    e[3].binding = 3; e[3].visibility = WGPUShaderStage_Fragment;
    e[3].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    e[3].buffer.minBindingSize = sizeof(GpuLight);
    e[4].binding = 4; e[4].visibility = WGPUShaderStage_Fragment;
    e[4].buffer.type = WGPUBufferBindingType_Uniform;
    e[4].buffer.minBindingSize = sizeof(FrameUniform);
    e[5] = e[1]; e[5].binding = 5;
    WGPUBindGroupLayoutDescriptor bgld = {};
    bgld.entryCount = 6; bgld.entries = e;
    bgl_ = wgpuDeviceCreateBindGroupLayout(device, &bgld);

    WGPUShaderModule fsm = make_shader(device, light_pass_wgsl());
    pipe_ = make_fullscreen_pipe(device, bgl_, fsm, fmt);
    wgpuShaderModuleRelease(fsm);
    return pipe_ != nullptr;
}

void Pass::run(WGPUCommandEncoder enc, WGPUTextureView dst, WGPUTextureView albedo,
               WGPUTextureView normal, WGPUTextureView occlusion) {
    WGPUBindGroupEntry b[6] = {};
    b[0].binding = 0; b[0].sampler = sampler_;
    b[1].binding = 1; b[1].textureView = albedo;
    b[2].binding = 2; b[2].textureView = normal ? normal : flat_normal_view_;
    b[3].binding = 3; b[3].buffer = lights_ssbo_;
    b[3].size = sizeof(GpuLight) * lights_;
    b[4].binding = 4; b[4].buffer = frame_ubo_;
    b[4].size = sizeof(FrameUniform);
    b[5].binding = 5; b[5].textureView = occlusion ? occlusion : open_occ_view_;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = bgl_; bgd.entryCount = 6; bgd.entries = b;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device_, &bgd);

    WGPURenderPassColorAttachment att = {};
    att.view = dst;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
    WGPURenderPassDescriptor pd = {};
    pd.colorAttachmentCount = 1; pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &pd);
    wgpuRenderPassEncoderSetPipeline(pass, pipe_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bg, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    wgpuBindGroupRelease(bg);
}

void Pass::shutdown() {
    if (pipe_) wgpuRenderPipelineRelease(pipe_);
    if (bgl_) wgpuBindGroupLayoutRelease(bgl_);
    if (open_occ_view_) wgpuTextureViewRelease(open_occ_view_);
    if (open_occ_) wgpuTextureRelease(open_occ_);
    if (flat_normal_view_) wgpuTextureViewRelease(flat_normal_view_);
    if (flat_normal_) wgpuTextureRelease(flat_normal_);
    if (sampler_) wgpuSamplerRelease(sampler_);
    if (frame_ubo_) wgpuBufferRelease(frame_ubo_);
    if (lights_ssbo_) wgpuBufferRelease(lights_ssbo_);
    pipe_ = nullptr; bgl_ = nullptr; open_occ_view_ = nullptr; open_occ_ = nullptr;
    flat_normal_view_ = nullptr; flat_normal_ = nullptr;
    sampler_ = nullptr; frame_ubo_ = nullptr; lights_ssbo_ = nullptr;
}

} // namespace lightgfx
