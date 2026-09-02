#include "validate_materials.hpp"

#include <cstdio>

#include "../../engine/material/table.hpp"
#include "../../engine/material/validate.hpp"
#include "baker_guid.hpp"

namespace asset {
namespace {

const AssetInput* find_asset(const std::vector<AssetInput>& assets, const char* name) {
    const uint64_t guid = bakers::guid_of(name);
    for (const AssetInput& a : assets)
        if (a.guid == guid) return &a;
    return nullptr;
}

} // namespace

bool validate_materials(const std::vector<AssetInput>& assets, const std::string& wgsl_path) {
    const AssetInput* table_asset = find_asset(assets, "materials");
    const AssetInput* wgsl_asset = find_asset(assets, "effects.wgsl");
    if (!table_asset || !wgsl_asset) {
        std::fprintf(stderr, "[assetc] materials: nothing to validate (section missing)\n");
        return false;
    }
    mat::Table table;
    const mat::LoadResult lr = table.load(table_asset->payload.data(), table_asset->payload.size());
    if (lr != mat::LoadResult::Ok) {
        std::fprintf(stderr, "[assetc] materials: baked table unreadable: %s\n",
                     mat::load_reason(lr));
        return false;
    }
    mat::ValidateResult res;
    const bool ok = mat::validate_library(
        wgsl_path, reinterpret_cast<const char*>(wgsl_asset->payload.data()), table, res);

    for (const mat::BackendReport& r : res.backends) {
        if (!r.available) {
            std::printf("[assetc] materials: %s skipped (%s)\n", r.backend.c_str(),
                        r.skip_reason.c_str());
            continue;
        }
        for (const mat::ShaderDiag& d : r.diags)
            std::fprintf(stderr, "%s\n", mat::format_diag(d).c_str());
        if (r.diags.empty())
            std::printf("[assetc] materials: %s validated %u material(s) in %u pipeline(s)\n",
                        r.backend.c_str(), res.materials, r.pipelines);
        else
            std::fprintf(stderr, "[assetc] materials: %s refused the library (%zu diagnostic(s))\n",
                         r.backend.c_str(), r.diags.size());
    }
    // Число ПРОВЕРЕННЫХ бэкендов печатается ВСЕГДА и отдельной строкой: гейт, ассертящий только
    // код возврата, зелен и на машине, где не поднялся ни один бэкенд, — а это «проверять было
    // негде», а не «шейдер валиден». Слово «checked», а не «validated»: бэкенд, ОТКАЗАВШИЙ
    // библиотеке, тоже проверил её, и считать его непроверенным значило бы прятать отказ.
    std::printf("[assetc] materials: %u of %zu target backend(s) checked\n", res.checked(),
                res.backends.size());
    return ok;
}

} // namespace asset
