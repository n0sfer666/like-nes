#include "normal_textures.hpp"

#include "../asset/hash.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace normgold {
namespace {

constexpr uint32_t TEX = 32;

// Круг спрайта из `material_textures.cpp`: за его пределами купола нет, там нормаль плоская.
constexpr float R2 = 0.2f;

uint8_t enc(float v) {
    const float t = (v * 0.5f + 0.5f) * 255.0f + 0.5f;
    return static_cast<uint8_t>(t < 0.0f ? 0.0f : (t > 255.0f ? 255.0f : t));
}

void put(uint8_t* p, float x, float y, float z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    const float k = len > 1e-6f ? 1.0f / len : 0.0f;
    p[0] = enc(x * k);
    p[1] = enc(y * k);
    p[2] = enc(z * k);
    p[3] = 255;
}

WGPUTexture upload(WGPUDevice device, WGPUQueue queue, const uint8_t* px, uint32_t side) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{side, side, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * side;
    layout.rowsPerImage = side;
    WGPUExtent3D ext = {side, side, 1};
    wgpuQueueWriteTexture(queue, &dst, px, 4u * side * side, &layout, &ext);
    return tex;
}

WGPUTexture make_flat(WGPUDevice device, WGPUQueue queue) {
    uint8_t px[4];
    put(px, 0.0f, 0.0f, 1.0f);
    return upload(device, queue, px, 1);
}

// Купол: полусфера, вписанная в круг спрайта. Нормаль полусферы радиуса r на расстоянии d от
// центра — (dx, dy, sqrt(r^2 - d^2)) / r; знак Y переворачивается ЗДЕСЬ, по контракту заголовка.
WGPUTexture make_dome(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / TEX - 0.5f;
            const float dy = (static_cast<float>(y) + 0.5f) / TEX - 0.5f;
            const float d2 = dx * dx + dy * dy;
            uint8_t* p = px.data() + 4 * (y * TEX + x);
            if (d2 >= R2) {
                put(p, 0.0f, 0.0f, 1.0f);
                continue;
            }
            put(p, dx, -dy, std::sqrt(R2 - d2));
        }
    }
    return upload(device, queue, px.data(), TEX);
}

// Вертикальные рёбра: рельеф меняется только по X, поэтому свет слева и свет справа дают РАЗНЫЙ
// кадр — купол на такое различие не отвечает, он симметричен.
WGPUTexture make_ridge(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / TEX;
            const float nx = 0.7f * std::sin(6.2831853f * 4.0f * u);
            uint8_t* p = px.data() + 4 * (y * TEX + x);
            put(p, nx, 0.0f, std::sqrt(1.0f - nx * nx));
        }
    }
    return upload(device, queue, px.data(), TEX);
}

} // namespace

bool Bank::init(WGPUDevice device, WGPUQueue queue) {
    flat_ = make_flat(device, queue);
    dome_ = make_dome(device, queue);
    ridge_ = make_ridge(device, queue);
    flat_view_ = wgpuTextureCreateView(flat_, nullptr);
    dome_view_ = wgpuTextureCreateView(dome_, nullptr);
    ridge_view_ = wgpuTextureCreateView(ridge_, nullptr);
    return flat_view_ && dome_view_ && ridge_view_;
}

namespace {

uint64_t guid_of(const char* name) { return asset::fnv1a(name, std::strlen(name)); }

} // namespace

WGPUTextureView Bank::view(uint64_t guid) const {
    if (guid == guid_of("sprite_normal")) return dome_view_;
    if (guid == guid_of("outline_normal")) return ridge_view_;
    return nullptr;
}

const char* Bank::name(uint64_t guid) const {
    if (guid == guid_of("sprite_normal")) return "sprite_normal";
    if (guid == guid_of("outline_normal")) return "outline_normal";
    return nullptr;
}

void Bank::shutdown() {
    if (ridge_view_) wgpuTextureViewRelease(ridge_view_);
    if (dome_view_) wgpuTextureViewRelease(dome_view_);
    if (flat_view_) wgpuTextureViewRelease(flat_view_);
    if (ridge_) wgpuTextureRelease(ridge_);
    if (dome_) wgpuTextureRelease(dome_);
    if (flat_) wgpuTextureRelease(flat_);
    ridge_view_ = nullptr; dome_view_ = nullptr; flat_view_ = nullptr;
    ridge_ = nullptr; dome_ = nullptr; flat_ = nullptr;
}

} // namespace normgold
