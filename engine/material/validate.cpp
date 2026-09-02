#include "validate.hpp"

#include <webgpu/webgpu.h>

#include <cstring>
#include <utility>

#include "../asset/hash.hpp"
#include "pipeline.hpp"
#include "validate_device.hpp"
#include "wgpu_error.hpp"

namespace mat {
namespace {

// Целевые бэкенды спеки #2: Metal, Vulkan, D3D12. На одной машине их столько, сколько дала ОС, —
// полный охват собирается тремя прогонами CI, а не одним.
const WGPUBackendType TARGETS[] = {WGPUBackendType_Metal, WGPUBackendType_Vulkan,
                                   WGPUBackendType_D3D12};

ShaderDiag missing_entry(const std::string& file, const char* entry) {
    ShaderDiag d;
    d.file = file;
    d.message = std::string("no entry point `") + entry + "` in the library module";
    return d;
}

void validate_on(WGPUDevice dev, const std::string& file, const char* wgsl, const Table& table,
                 BackendReport& r) {
    detail::error_scope_begin(dev);
    WGPUShaderModule module = detail::make_module(dev, wgsl);
    std::string module_err = detail::error_scope_end(dev);
    if (!module_err.empty()) {
        r.diags.push_back(parse_wgpu_error(file, module_err));
        if (module) wgpuShaderModuleRelease(module);
        return;   // пайплайны по несобравшемуся модулю дали бы отказ на каждый материал разом
    }
    WGPUBindGroupLayout bgl = detail::make_bind_group_layout(dev);
    WGPUPipelineLayoutDescriptor pld = {};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(dev, &pld);

    // Ключ тот же, что у кэша (точка входа + смешивание): валидатор обязан проверить ровно те
    // пайплайны, что построит рантайм, — ни больше, ни меньше.
    std::vector<std::pair<uint64_t, uint8_t>> seen;
    for (uint32_t i = 0; i < table.count(); ++i) {
        const MaterialRow& row = table.row(i);
        const char* entry = table.shader(i);
        const std::pair<uint64_t, uint8_t> key{asset::fnv1a(entry, std::strlen(entry)), row.blend};
        bool dup = false;
        for (const auto& k : seen) dup = dup || k == key;
        if (dup) continue;
        seen.push_back(key);
        if (!detail::has_entry(wgsl, entry)) {
            r.diags.push_back(missing_entry(file, entry));
            continue;
        }
        detail::error_scope_begin(dev);
        // Формат цели фиксирован: библиотека рисуется в RGBA8Unorm и в golden-харнессе, и в
        // офскрине игры. Проверять её в формате поверхности значило бы проверять другой пайплайн.
        WGPURenderPipeline p = detail::make_pipeline(dev, layout, module, entry, row.blend,
                                                     WGPUTextureFormat_RGBA8Unorm);
        std::string err = detail::error_scope_end(dev);
        if (err.empty()) ++r.pipelines;
        else r.diags.push_back(parse_wgpu_error(file, err));
        if (p) wgpuRenderPipelineRelease(p);
    }
    wgpuPipelineLayoutRelease(layout);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuShaderModuleRelease(module);
}

} // namespace

uint32_t ValidateResult::checked() const {
    uint32_t n = 0;
    for (const BackendReport& r : backends) n += r.available ? 1u : 0u;
    return n;
}

uint32_t ValidateResult::diagnostics() const {
    uint32_t n = 0;
    for (const BackendReport& r : backends) n += static_cast<uint32_t>(r.diags.size());
    return n;
}

bool validate_library(const std::string& file, const char* wgsl, const Table& table,
                      ValidateResult& out) {
    out.backends.clear();
    out.materials = table.count();
    if (!wgsl) {
        BackendReport r;
        r.backend = "none";
        r.diags.push_back(ShaderDiag{file, 0, 0, "library module is empty"});
        out.backends.push_back(std::move(r));
        return false;
    }
    for (WGPUBackendType b : TARGETS) {
        BackendReport r;
        r.backend = backend_name(b);
        ValidationDevice vd;
        if (!vd.open(b, r.skip_reason)) {
            out.backends.push_back(std::move(r));
            continue;
        }
        r.available = true;
        validate_on(vd.device, file, wgsl, table, r);
        vd.close();
        out.backends.push_back(std::move(r));
    }
    return out.diagnostics() == 0;
}

} // namespace mat
