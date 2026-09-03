#include "normal_pass.hpp"

#include "../material/pipeline.hpp"
#include "shaders_normal.hpp"

namespace normgfx {
namespace {

WGPUBuffer make_buffer(WGPUDevice device, WGPUQueue queue, WGPUBufferUsageFlags usage,
                       const void* data, std::size_t size) {
    WGPUBufferDescriptor bd = {};
    bd.usage = usage | WGPUBufferUsage_CopyDst;
    bd.size = size;
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &bd);
    if (data) wgpuQueueWriteBuffer(queue, buf, 0, data, size);
    return buf;
}

} // namespace

bool Pass::init(WGPUDevice device, WGPUQueue queue, const mat::Table& table,
                const normgold::Bank& bank, uint32_t w, uint32_t h, WGPUTextureView albedo,
                WGPUTextureFormat fmt) {
    device_ = device;
    queue_ = queue;

    bgl_ = mat::detail::make_bind_group_layout(device);
    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = &bgl_;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUShaderModule module = mat::detail::make_module(device, normal_pass_wgsl());
    pipe_ = mat::detail::make_pipeline(device, layout, module, "fs_normal",
                                       static_cast<uint8_t>(mat::Blend::Alpha), fmt);
    wgpuShaderModuleRelease(module);
    wgpuPipelineLayoutRelease(layout);
    if (!pipe_) return false;

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
                            sizeof(mat::Instance) * 64);
    const float vp[4] = {static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f, 0.0f, 0.0f};
    vp_ubo_ = make_buffer(device, queue, WGPUBufferUsage_Uniform, vp, sizeof(vp));

    WGPUSamplerDescriptor sd = {};
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    sd.magFilter = WGPUFilterMode_Nearest;
    sd.minFilter = WGPUFilterMode_Nearest;
    sd.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sd.lodMaxClamp = 1.0f;
    sd.maxAnisotropy = 1;
    sampler_ = wgpuDeviceCreateSampler(device, &sd);

    // Слот адресуется ИМЕНЕМ `normal`, а ассет — guid'ом из той же строки таблицы. Ни номер слота,
    // ни имя ассета в этот файл не зашиты: обе величины приходят из `library.mat`.
    for (uint32_t m = 0; m < table.count(); ++m) {
        Group g;
        const int32_t ti = table.texture_of(m, "normal");
        WGPUTextureView map = nullptr;
        if (ti < 0) {
            ++flat_;
        } else if ((map = bank.view(table.texture(static_cast<uint32_t>(ti)).guid)) != nullptr) {
            g.asset = bank.name(table.texture(static_cast<uint32_t>(ti)).guid);
            ++mapped_;
        } else {
            ++missing_;
        }

        WGPUBindGroupEntry b[4] = {};
        b[0].binding = 0; b[0].buffer = vp_ubo_; b[0].size = 16;
        b[1].binding = 1; b[1].sampler = sampler_;
        b[2].binding = 2; b[2].textureView = albedo;
        b[3].binding = 3; b[3].textureView = map ? map : bank.flat();
        WGPUBindGroupDescriptor bgd = {};
        bgd.layout = bgl_; bgd.entryCount = 4; bgd.entries = b;
        g.bg = wgpuDeviceCreateBindGroup(device, &bgd);
        if (!g.bg) return false;
        groups_.push_back(g);
    }
    return true;
}

void Pass::build(const matgold::Scene& scene) {
    std::vector<mat::Instance> ordered;
    ordered.reserve(scene.items().size());
    for (uint32_t m = 0; m < groups_.size(); ++m) {
        Group& g = groups_[m];
        g.first = static_cast<uint32_t>(ordered.size());
        g.count = 0;
        for (uint32_t i = 0; i < scene.instances(); ++i) {
            if (scene.item_material(i) != m) continue;
            ordered.push_back(scene.items()[i]);
            ++g.count;
        }
    }
    instances_ = static_cast<uint32_t>(ordered.size());
    if (instances_ != 0)
        wgpuQueueWriteBuffer(queue_, inst_vbo_, 0, ordered.data(),
                             ordered.size() * sizeof(mat::Instance));
}

void Pass::run(WGPUCommandEncoder enc, WGPUTextureView dst) {
    WGPURenderPassColorAttachment att = {};
    att.view = dst;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = CLEAR;
    WGPURenderPassDescriptor pd = {};
    pd.colorAttachmentCount = 1;
    pd.colorAttachments = &att;
    WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(enc, &pd);
    if (instances_ != 0) {
        wgpuRenderPassEncoderSetPipeline(rp, pipe_);
        wgpuRenderPassEncoderSetVertexBuffer(rp, 0, quad_vbo_, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetVertexBuffer(rp, 1, inst_vbo_, 0,
                                             instances_ * sizeof(mat::Instance));
        wgpuRenderPassEncoderSetIndexBuffer(rp, quad_ibo_, WGPUIndexFormat_Uint16, 0,
                                            WGPU_WHOLE_SIZE);
        for (const Group& g : groups_) {
            if (g.count == 0) continue;
            wgpuRenderPassEncoderSetBindGroup(rp, 0, g.bg, 0, nullptr);
            wgpuRenderPassEncoderDrawIndexed(rp, 6, g.count, 0, 0, g.first);
        }
    }
    wgpuRenderPassEncoderEnd(rp);
    wgpuRenderPassEncoderRelease(rp);
}

const char* Pass::asset(uint32_t material) const {
    return material < groups_.size() ? groups_[material].asset : nullptr;
}

void Pass::shutdown() {
    for (Group& g : groups_)
        if (g.bg) wgpuBindGroupRelease(g.bg);
    groups_.clear();
    if (sampler_) wgpuSamplerRelease(sampler_);
    if (vp_ubo_) wgpuBufferRelease(vp_ubo_);
    if (inst_vbo_) wgpuBufferRelease(inst_vbo_);
    if (quad_ibo_) wgpuBufferRelease(quad_ibo_);
    if (quad_vbo_) wgpuBufferRelease(quad_vbo_);
    if (pipe_) wgpuRenderPipelineRelease(pipe_);
    if (bgl_) wgpuBindGroupLayoutRelease(bgl_);
    sampler_ = nullptr; vp_ubo_ = nullptr; inst_vbo_ = nullptr; quad_ibo_ = nullptr;
    quad_vbo_ = nullptr; pipe_ = nullptr; bgl_ = nullptr;
    instances_ = 0;
}

} // namespace normgfx
