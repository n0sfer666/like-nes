#include "material_fx.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "asset_manager.hpp"
#include "hash.hpp"

namespace game {
namespace {

bool wait_ready(asset::AssetManager& am, uint64_t guid) {
    for (int f = 0; f < 500; ++f) {
        am.sync_point();
        if (am.is_ready(guid)) return true;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return false;
}

uint64_t guid_of(const char* name) { return asset::fnv1a(name, std::strlen(name)); }

// Шум для растворения — СОДЕРЖИМОЕ игры, а не часть библиотеки: материал объявляет слот `noise`,
// а какой именно узор в него попадёт, решает тот, кто рисует. Детерминированный, потому что кадр
// игры сверяется голденом.
WGPUTextureView make_noise(WGPUDevice device, WGPUQueue queue, WGPUTexture& tex) {
    constexpr uint32_t N = 64;
    std::vector<uint8_t> px(N * N * 4);
    for (uint32_t y = 0; y < N; ++y) {
        for (uint32_t x = 0; x < N; ++x) {
            uint32_t h = x * 374761393u + y * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            const uint8_t v = static_cast<uint8_t>((h >> 16) & 0xffu);
            uint8_t* p = &px[(y * N + x) * 4];
            p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
        }
    }
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{N, N, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1; td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    tex = wgpuDeviceCreateTexture(device, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex; dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * N; layout.rowsPerImage = N;
    WGPUExtent3D ext = {N, N, 1};
    wgpuQueueWriteTexture(queue, &dst, px.data(), px.size(), &layout, &ext);
    return wgpuTextureCreateView(tex, nullptr);
}

} // namespace

struct MaterialFx::Bundle {
    asset::AssetManager am;
    ~Bundle() { am.close(); }
};

bool MaterialFx::init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target,
                      const char* bundle_path) {
    auto b = std::make_shared<Bundle>();
    if (!b->am.open(bundle_path, 8u * 1024 * 1024, /*trusted=*/false)) {
        std::fprintf(stderr, "[game] materials: bundle '%s' unreadable\n", bundle_path);
        return false;
    }
    const uint64_t g_tab = guid_of("materials");
    const uint64_t g_wgsl = guid_of("effects.wgsl");
    b->am.request(g_tab);
    b->am.request(g_wgsl);
    if (!wait_ready(b->am, g_tab) || !wait_ready(b->am, g_wgsl)) {
        std::fprintf(stderr, "[game] materials: sections never became ready\n");
        return false;
    }
    const asset::Loaded tab = b->am.get(g_tab);
    const mat::LoadResult lr = table_.load(tab.data, tab.size);
    if (lr != mat::LoadResult::Ok) {
        std::fprintf(stderr, "[game] materials: table unusable: %s\n", mat::load_reason(lr));
        return false;
    }
    const asset::Loaded src = b->am.get(g_wgsl);
    wgsl_.assign(reinterpret_cast<const char*>(src.data), src.size ? src.size - 1 : 0);

    mat::CacheDesc d;
    d.device = device; d.queue = queue; d.target = target;
    d.table = &table_; d.wgsl = wgsl_.c_str();
    if (!cache_.init(d)) {
        std::fprintf(stderr, "[game] materials: cache refused to start\n");
        return false;
    }
    // Прогрев ЗДЕСЬ, а не при первом использовании материала: компиляция в кадре и есть тот фриз,
    // который запрещает инвариант 3 спеки #18, и заметен он ровно там, где эффект включается —
    // в момент удара, взрыва или смерти.
    cache_.warm_up();
    noise_view_ = make_noise(device, queue, noise_);
    bundle_ = std::move(b);
    ready_ = true;
    return true;
}

void MaterialFx::params(uint32_t material, float out[mat::PARAM_BLOCK_FLOATS]) const {
    if (material >= table_.count()) {
        for (uint32_t i = 0; i < mat::PARAM_BLOCK_FLOATS; ++i) out[i] = 0.0f;
        return;
    }
    table_.resolve(material, out);
}

void MaterialFx::shutdown() {
    if (noise_view_) { wgpuTextureViewRelease(noise_view_); noise_view_ = nullptr; }
    if (noise_) { wgpuTextureRelease(noise_); noise_ = nullptr; }
    cache_.shutdown();
    bundle_.reset();
    ready_ = false;
}

} // namespace game
