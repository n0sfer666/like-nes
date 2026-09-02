#include <cstring>
#include <string>
#include <vector>

#include "../asset/hash.hpp"
#include "cache.hpp"
#include "pipeline.hpp"
#include "wgpu_error.hpp"

// Горячая замена библиотеки (гейт 3 спеки #18) — отдельным TU от `cache.cpp` по предмету: у кэша
// вопрос «какой пайплайн у этого материала», здесь — «чем заменить ВЕСЬ набор, ничего не сломав».
//
// Собирается ПОЛНЫЙ набор на стороне и только потом меняется местами. Замена по одному пайплайну
// оставила бы после битой правки половину библиотеки от нового текста, а половину от старого — и
// это состояние хуже обоих: предыдущий вариант в нём уже не жив, а новый ещё не работает.
namespace mat {
namespace {

struct Staged {
    PipelineKey key;
    WGPURenderPipeline pipe;
};

void release(std::vector<Staged>& staged) {
    for (const Staged& s : staged) wgpuRenderPipelineRelease(s.pipe);
    staged.clear();
}

} // namespace

bool Cache::reload(const char* wgsl, const std::string& file, ShaderDiag& diag) {
    if (!device_ || !table_ || !wgsl) {
        diag = ShaderDiag{file, 0, 0, "hot-reload asked of a cache that never started"};
        return false;
    }
    detail::error_scope_begin(device_);
    WGPUShaderModule module = detail::make_module(device_, wgsl);
    const std::string module_err = detail::error_scope_end(device_);
    // Указатель тут ничего не значит: битый модуль приезжает НЕНУЛЕВЫМ, и правду говорит только
    // область ошибки — та же улика, на которой стоит валидатор бейка.
    if (!module_err.empty()) {
        diag = parse_wgpu_error(file, module_err);
        if (module) wgpuShaderModuleRelease(module);
        return false;
    }

    std::vector<Staged> staged;
    bool ok = true;
    for (uint32_t i = 0; i < table_->count() && ok; ++i) {
        const MaterialRow& row = table_->row(i);
        const char* entry = table_->shader(i);
        const PipelineKey key{asset::fnv1a(entry, std::strlen(entry)), row.blend};
        bool dup = false;
        for (const Staged& s : staged) dup = dup || (s.key.entry == key.entry && s.key.blend == key.blend);
        if (dup) continue;
        if (!detail::has_entry(wgsl, entry)) {
            diag = ShaderDiag{file, 0, 0,
                              std::string("no entry point `") + entry + "` in the library module"};
            ok = false;
            break;
        }
        detail::error_scope_begin(device_);
        WGPURenderPipeline p = detail::make_pipeline(device_, layout_, module, entry, row.blend,
                                                     target_);
        const std::string err = detail::error_scope_end(device_);
        if (!err.empty()) {
            diag = parse_wgpu_error(file, err);
            if (p) wgpuRenderPipelineRelease(p);
            ok = false;
            break;
        }
        staged.push_back(Staged{key, p});
    }
    if (!ok) {
        release(staged);
        wgpuShaderModuleRelease(module);
        return false;
    }

    for (const Entry& e : entries_) {
        if (e.pipe && e.pipe != fallback_) wgpuRenderPipelineRelease(e.pipe);
    }
    entries_.clear();
    for (const Staged& s : staged) entries_.push_back(Entry{s.key, s.pipe});
    if (module_) wgpuShaderModuleRelease(module_);
    module_ = module;
    // Текст берётся В СОБСТВЕННОСТЬ: прежний `wgsl_` указывал в mmap-регион бандла, а новый пришёл
    // из буфера того, кто читал файл, и переживать вызов он не обязан. `has_entry` смотрит в него
    // на каждом промахе кэша, поэтому висячий указатель здесь читался бы как «нет точки входа».
    owned_wgsl_ = wgsl;
    wgsl_ = owned_wgsl_.c_str();
    created_ += static_cast<uint32_t>(staged.size());
    ++reloads_;
    return true;
}

} // namespace mat
