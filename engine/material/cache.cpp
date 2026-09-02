#include "cache.hpp"

#include <cstdio>
#include <cstring>

#include "../asset/hash.hpp"
#include "pipeline.hpp"

namespace mat {
namespace {

WGPUBindGroupLayout make_layout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry e[4] = {};
    e[0].binding = 0;
    e[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    e[0].buffer.type = WGPUBufferBindingType_Uniform;
    e[0].buffer.minBindingSize = 16;
    e[1].binding = 1;
    e[1].visibility = WGPUShaderStage_Fragment;
    e[1].sampler.type = WGPUSamplerBindingType_Filtering;
    e[2].binding = 2;
    e[2].visibility = WGPUShaderStage_Fragment;
    e[2].texture.sampleType = WGPUTextureSampleType_Float;
    e[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[3] = e[2];
    e[3].binding = 3;

    WGPUBindGroupLayoutDescriptor d = {};
    d.entryCount = 4;
    d.entries = e;
    return wgpuDeviceCreateBindGroupLayout(device, &d);
}

} // namespace

bool Cache::init(const CacheDesc& d) {
    if (!d.device || !d.table) return false;
    device_ = d.device;
    target_ = d.target;
    table_ = d.table;
    wgsl_ = d.wgsl;

    bgl_ = make_layout(device_);
    WGPUPipelineLayoutDescriptor pl = {};
    pl.bindGroupLayoutCount = 1;
    pl.bindGroupLayouts = &bgl_;
    layout_ = wgpuDeviceCreatePipelineLayout(device_, &pl);

    // Запасной путь поднимается ПЕРВЫМ и до всякого материала: кэш, который создаёт запасное только
    // в момент отказа, отказывает дважды подряд там, где отказал один раз.
    fallback_module_ = detail::make_module(device_, detail::fallback_wgsl());
    fallback_ = fallback_module_ ? detail::make_pipeline(device_, layout_, fallback_module_,
                                                         "fs_fallback", 1, target_)
                                 : nullptr;
    if (!fallback_) {
        std::fprintf(stderr,
                     "[material] fallback pipeline failed to build: nothing to draw with\n");
        return false;
    }
    module_ = wgsl_ ? detail::make_module(device_, wgsl_) : nullptr;
    if (!module_)
        std::fprintf(stderr,
                     "[material] effect library failed to build: every material falls back\n");
    return true;
}

void Cache::shutdown() {
    for (const Entry& e : entries_) {
        if (e.pipe && e.pipe != fallback_) wgpuRenderPipelineRelease(e.pipe);
    }
    entries_.clear();
    if (fallback_) wgpuRenderPipelineRelease(fallback_);
    if (module_) wgpuShaderModuleRelease(module_);
    if (fallback_module_) wgpuShaderModuleRelease(fallback_module_);
    if (layout_) wgpuPipelineLayoutRelease(layout_);
    if (bgl_) wgpuBindGroupLayoutRelease(bgl_);
    fallback_ = nullptr;
    module_ = nullptr;
    fallback_module_ = nullptr;
    layout_ = nullptr;
    bgl_ = nullptr;
    device_ = nullptr;
    table_ = nullptr;
    // Счётчики гасятся вместе с объектами, которые они считали: иначе повторный `init` печатал бы
    // «3 pipeline(s)» ещё до первого прогрева, а число это стоит утверждением в гейте 8.
    created_ = 0;
    fallbacks_ = 0;
}

WGPURenderPipeline Cache::find(const PipelineKey& k) const {
    for (const Entry& e : entries_) {
        if (e.key.entry == k.entry && e.key.blend == k.blend) return e.pipe;
    }
    return nullptr;
}

WGPURenderPipeline Cache::build(const char* entry, uint8_t blend) {
    if (!module_ || !detail::has_entry(wgsl_, entry)) {
        std::fprintf(stderr, "[material] no entry point `%s` in the library: falling back\n",
                     entry);
        return nullptr;
    }
    WGPURenderPipeline p = detail::make_pipeline(device_, layout_, module_, entry, blend, target_);
    if (!p) {
        std::fprintf(stderr, "[material] pipeline `%s` refused by the validator: falling back\n", entry);
        return nullptr;
    }
    ++created_;
    return p;
}

WGPURenderPipeline Cache::pipeline(uint32_t material) {
    if (!table_ || material >= table_->count()) {
        ++fallbacks_;
        return fallback_;
    }
    const MaterialRow& r = table_->row(material);
    const char* entry = table_->shader(material);
    const PipelineKey key{asset::fnv1a(entry, std::strlen(entry)), r.blend};
    if (WGPURenderPipeline hit = find(key)) {
        if (hit == fallback_) ++fallbacks_;
        return hit;
    }
    WGPURenderPipeline p = build(entry, r.blend);
    if (!p) {
        // Отказ запоминается ТОЙ ЖЕ записью: иначе материал со сломанным шейдером тратит попытку
        // компиляции каждый кадр, и `pipelines_created()` перестаёт быть утверждением о разогреве.
        ++fallbacks_;
        p = fallback_;
    }
    entries_.push_back(Entry{key, p});
    return p;
}

void Cache::warm_up() {
    if (!table_) return;
    for (uint32_t i = 0; i < table_->count(); ++i) pipeline(i);
}

} // namespace mat
