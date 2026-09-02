#include "material_textures.hpp"

#include <cstdint>
#include <vector>

namespace matgold {
namespace {

constexpr uint32_t TEX = 32;

WGPUTexture upload(WGPUDevice device, WGPUQueue queue, const uint8_t* px) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{TEX, TEX, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * TEX;
    layout.rowsPerImage = TEX;
    WGPUExtent3D ext = {TEX, TEX, 1};
    wgpuQueueWriteTexture(queue, &dst, px, 4u * TEX * TEX, &layout, &ext);
    return tex;
}

} // namespace

WGPUTexture make_sprite_texture(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / TEX - 0.5f;
            const float dy = (static_cast<float>(y) + 0.5f) / TEX - 0.5f;
            uint8_t* p = px.data() + 4 * (y * TEX + x);
            p[0] = static_cast<uint8_t>(60 + 5 * x);
            p[1] = static_cast<uint8_t>(90 + 4 * y);
            p[2] = 200;
            p[3] = dx * dx + dy * dy <= 0.2f ? 255 : 0;
        }
    }
    return upload(device, queue, px.data());
}

WGPUTexture make_noise_texture(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    uint32_t h = 0x9e3779b9u;
    for (uint32_t i = 0; i < TEX * TEX; ++i) {
        h ^= i + 0x165667b1u + (h << 6) + (h >> 2);
        h *= 0x27d4eb2fu;
        const uint8_t v = static_cast<uint8_t>((h >> 13) & 0xffu);
        uint8_t* p = px.data() + 4 * i;
        p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
    }
    return upload(device, queue, px.data());
}

} // namespace matgold
