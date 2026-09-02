#include "batch.hpp"

#include "material_runs.hpp"
#include "sprite_pipeline.hpp"
#include "world.hpp"

namespace game {
namespace {

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
                       const Atlas& atlas, MaterialFx* fx) {
    device_ = device; queue_ = queue;
    fx_ = (fx != nullptr && fx->ready()) ? fx : nullptr;
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

    // Раскладка привязки БЕРЁТСЯ У КЭША, когда игра рисует материалами: пайплайны библиотеки
    // построены на ней, и группа, собранная по своей копии, подошла бы им только случайно. Своя
    // остаётся для прогона без библиотеки — тогда четвёртой привязки просто нет.
    if (fx_) {
        bgl_ = fx_->layout();
    } else {
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
        owns_bgl_ = true;
    }

    WGPUBindGroupEntry b[4] = {};
    b[0].binding = 0; b[0].buffer = vp_ubo_; b[0].size = 16;
    b[1].binding = 1; b[1].sampler = sampler_;
    b[2].binding = 2; b[2].textureView = view_;
    b[3].binding = 3; b[3].textureView = fx_ ? fx_->noise_view() : nullptr;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = bgl_; bgd.entryCount = fx_ ? 4u : 3u; bgd.entries = b;
    bg_ = wgpuDeviceCreateBindGroup(device, &bgd);

    pipe_ = make_sprite_pipeline(device, target, bgl_);
    mat_ = std::make_unique<uint16_t[]>(MAX_INSTANCES);
    runs_ = std::make_unique<MaterialRun[]>(MAX_INSTANCES);
    set_viewport(VIEW_W, VIEW_H);
}

void SpriteBatch::set_viewport(uint32_t w, uint32_t h) {
    const float vp[4] = {(float)w * 0.5f, (float)h * 0.5f, 0, 0};
    wgpuQueueWriteBuffer(queue_, vp_ubo_, 0, vp, sizeof(vp));
}

void SpriteBatch::begin() { stage_.begin(); draws_ = 0; }

void SpriteBatch::push(const Instance& inst) { push(inst, NO_MATERIAL); }

// Материал запоминается ПАРАЛЛЕЛЬНЫМ массивом, а не полем инстанса: инстанс — это вершинные
// данные, и лишнее поле в нём уехало бы на видеокарту, где никому не нужно. Индекс берётся у
// накопителя ПОСЛЕ попытки: переполненный кадр роняет спрайт, и материал обязан упасть вместе с
// ним, иначе прогоны разъедутся с инстансами на единицу и раскрасят не тех.
void SpriteBatch::push(const Instance& inst, uint32_t material) {
    const uint32_t before = stage_.count();
    stage_.push(inst);
    if (stage_.count() > before) mat_[before] = static_cast<uint16_t>(material);
}

// Прогоны режутся по ПОРЯДКУ, а не сортировкой по пайплайну: в 2D порядок отрисовки и есть
// перекрытие, и группировка, переставившая спрайты местами, меняет картинку. Поэтому вызовов
// ровно столько, сколько раз материал сменился подряд: одинаковые соседи сливаются, а разные
// платят — это и видно счётчиком `draws()`.
void SpriteBatch::flush(WGPURenderPassEncoder pass) {
    if (stage_.count() == 0) return;
    wgpuQueueWriteBuffer(queue_, inst_vbo_, 0, stage_.data(), stage_.bytes());
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bg_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, quad_vbo_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, inst_vbo_, 0, stage_.bytes());
    wgpuRenderPassEncoderSetIndexBuffer(pass, quad_ibo_, WGPUIndexFormat_Uint16, 0,
                                        WGPU_WHOLE_SIZE);
    draws_ = material_runs(mat_.get(), stage_.count(), runs_.get(), MAX_INSTANCES);
    for (uint32_t i = 0; i < draws_; ++i) {
        const MaterialRun& r = runs_[i];
        WGPURenderPipeline pipe = pipe_;
        if (fx_ && r.material != NO_MATERIAL) pipe = fx_->pipeline(r.material);
        wgpuRenderPassEncoderSetPipeline(pass, pipe);
        wgpuRenderPassEncoderDrawIndexed(pass, 6, r.count, 0, 0, r.first);
    }
}

void SpriteBatch::shutdown() {
    if (pipe_) wgpuRenderPipelineRelease(pipe_);
    if (bg_) wgpuBindGroupRelease(bg_);
    if (bgl_ && owns_bgl_) wgpuBindGroupLayoutRelease(bgl_);
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
