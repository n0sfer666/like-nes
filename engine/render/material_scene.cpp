#include "material_scene.hpp"

#include "material_textures.hpp"

namespace matgold {
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

uint8_t slot_unit(const mat::Table& t, uint32_t m, uint8_t slot) {
    for (uint32_t hops = 0; m < t.count() && hops <= t.count(); ++hops) {
        const mat::MaterialRow& r = t.row(m);
        for (uint16_t i = 0; i < r.param_count; ++i) {
            const mat::ParamRow& p = t.param(r.param_first + i);
            if (p.slot == slot) return p.unit;
        }
        if (r.base == mat::NO_BASE) break;
        m = r.base;
    }
    return 0;
}

} // namespace

bool Scene::init(WGPUDevice device, WGPUQueue queue, uint32_t w, uint32_t h,
                 WGPUBindGroupLayout bgl) {
    device_ = device;
    queue_ = queue;

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

    albedo_ = make_sprite_texture(device, queue);
    aux_ = make_noise_texture(device, queue);
    albedo_view_ = wgpuTextureCreateView(albedo_, nullptr);
    aux_view_ = wgpuTextureCreateView(aux_, nullptr);

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

    WGPUBindGroupEntry b[4] = {};
    b[0].binding = 0; b[0].buffer = vp_ubo_; b[0].size = 16;
    b[1].binding = 1; b[1].sampler = sampler_;
    b[2].binding = 2; b[2].textureView = albedo_view_;
    b[3].binding = 3; b[3].textureView = aux_view_;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = bgl; bgd.entryCount = 4; bgd.entries = b;
    bg_ = wgpuDeviceCreateBindGroup(device, &bgd);
    return bg_ != nullptr;
}

void Scene::build(const mat::Table& t, mat::Cache& cache) {
    items_.clear();
    batches_.clear();
    const uint32_t cols = t.count();
    std::vector<WGPURenderPipeline> pipes;
    std::vector<mat::Instance> per_material;
    for (uint32_t m = 0; m < cols; ++m) {
        float block[mat::PARAM_BLOCK_FLOATS];
        t.resolve(m, block);
        const float step = slot_unit(t, m, 4) == static_cast<uint8_t>(mat::Unit::Pixels)
                               ? 1.0f
                               : 0.25f;
        for (uint32_t k = 0; k < PER_MATERIAL; ++k) {
            mat::Instance inst;
            inst.x = -320.0f + 640.0f * (static_cast<float>(m) + 0.5f) / static_cast<float>(cols);
            inst.y = -180.0f + 90.0f * (static_cast<float>(k) + 0.5f);
            inst.w = 64.0f;
            inst.h = 64.0f;
            for (uint32_t i = 0; i < mat::PARAM_BLOCK_FLOATS; ++i) inst.params[i] = block[i];
            inst.params[4] = block[4] + step * static_cast<float>(k);
            per_material.push_back(inst);
            pipes.push_back(cache.pipeline(m));
        }
    }
    // Порядок инстансов задаётся ПАЙПЛАЙНОМ, а не материалом: инстансы `flash_red` и `flash_gold`
    // делят точку входа с `flash`, и группировка обязана свести их в один вызов — иначе счётчик
    // гейта 5 меряет порядок обхода таблицы, а не батчинг.
    for (std::size_t i = 0; i < pipes.size(); ++i) {
        bool seen = false;
        for (const Batch& b : batches_) seen = seen || b.pipe == pipes[i];
        if (seen) continue;
        Batch b{pipes[i], static_cast<uint32_t>(items_.size()), 0};
        for (std::size_t j = 0; j < pipes.size(); ++j) {
            if (pipes[j] != pipes[i]) continue;
            items_.push_back(per_material[j]);
            ++b.count;
        }
        batches_.push_back(b);
    }
    wgpuQueueWriteBuffer(queue_, inst_vbo_, 0, items_.data(),
                         items_.size() * sizeof(mat::Instance));
}

uint32_t Scene::draw(WGPURenderPassEncoder pass) {
    if (items_.empty()) return 0;
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bg_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, quad_vbo_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, inst_vbo_, 0,
                                         items_.size() * sizeof(mat::Instance));
    wgpuRenderPassEncoderSetIndexBuffer(pass, quad_ibo_, WGPUIndexFormat_Uint16, 0,
                                        WGPU_WHOLE_SIZE);
    for (const Batch& b : batches_) {
        wgpuRenderPassEncoderSetPipeline(pass, b.pipe);
        wgpuRenderPassEncoderDrawIndexed(pass, 6, b.count, 0, 0, b.first);
    }
    return static_cast<uint32_t>(batches_.size());
}

void Scene::shutdown() {
    if (bg_) wgpuBindGroupRelease(bg_);
    if (sampler_) wgpuSamplerRelease(sampler_);
    if (aux_view_) wgpuTextureViewRelease(aux_view_);
    if (albedo_view_) wgpuTextureViewRelease(albedo_view_);
    if (aux_) wgpuTextureRelease(aux_);
    if (albedo_) wgpuTextureRelease(albedo_);
    if (vp_ubo_) wgpuBufferRelease(vp_ubo_);
    if (inst_vbo_) wgpuBufferRelease(inst_vbo_);
    if (quad_ibo_) wgpuBufferRelease(quad_ibo_);
    if (quad_vbo_) wgpuBufferRelease(quad_vbo_);
    bg_ = nullptr; sampler_ = nullptr; aux_view_ = nullptr; albedo_view_ = nullptr;
    aux_ = nullptr; albedo_ = nullptr; vp_ubo_ = nullptr; inst_vbo_ = nullptr;
    quad_ibo_ = nullptr; quad_vbo_ = nullptr;
}

} // namespace matgold
