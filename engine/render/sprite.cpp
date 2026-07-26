#include "sprite.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t TEX = 256;

WGPUTexture upload_rgba8(WGPUDevice device, WGPUQueue queue, uint32_t w, uint32_t h,
                         const std::vector<uint8_t>& px, WGPUTextureView* out_view) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{w, h, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td);

    WGPUImageCopyTexture dst = {};
    dst.texture = tex;
    dst.mipLevel = 0;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * w;
    layout.rowsPerImage = h;
    WGPUExtent3D ext = {w, h, 1};
    wgpuQueueWriteTexture(queue, &dst, px.data(), px.size(), &layout, &ext);

    *out_view = wgpuTextureCreateView(tex, nullptr);
    return tex;
}

// Bevel-высота: плато в центре, спад к краям квадрата (по chebyshev-расстоянию).
float bevel_height(float u, float v) {
    const float dx = std::fabs(u - 0.5f) * 2.0f;
    const float dy = std::fabs(v - 0.5f) * 2.0f;
    const float d = std::fmax(dx, dy);
    const float edge = 0.72f;
    if (d < edge) return 1.0f;
    const float k = (d - edge) / (1.0f - edge);
    return 1.0f - k * k;
}

std::vector<uint8_t> gen_albedo() {
    std::vector<uint8_t> px(TEX * TEX * 4);
    for (uint32_t y = 0; y < TEX; ++y)
        for (uint32_t x = 0; x < TEX; ++x) {
            const float u = (x + 0.5f) / TEX, v = (y + 0.5f) / TEX;
            const bool check = ((x >> 5) ^ (y >> 5)) & 1;
            const float base = check ? 0.82f : 0.62f;
            uint8_t* p = &px[(y * TEX + x) * 4];
            p[0] = (uint8_t)(base * 235);
            p[1] = (uint8_t)(base * 205);
            p[2] = (uint8_t)(base * 250);
            p[3] = (uint8_t)(bevel_height(u, v) > 0.02f ? 255 : 0);
        }
    return px;
}

std::vector<uint8_t> gen_normal() {
    std::vector<uint8_t> px(TEX * TEX * 4);
    const float texel = 1.0f / TEX;
    for (uint32_t y = 0; y < TEX; ++y)
        for (uint32_t x = 0; x < TEX; ++x) {
            const float u = (x + 0.5f) / TEX, v = (y + 0.5f) / TEX;
            const float hl = bevel_height(u - texel, v);
            const float hr = bevel_height(u + texel, v);
            const float hd = bevel_height(u, v - texel);
            const float hu = bevel_height(u, v + texel);
            float nx = (hl - hr);
            float ny = (hd - hu);
            float nz = 0.14f;
            const float inv = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            nx *= inv; ny *= inv; nz *= inv;
            uint8_t* p = &px[(y * TEX + x) * 4];
            p[0] = (uint8_t)((nx * 0.5f + 0.5f) * 255);
            p[1] = (uint8_t)((ny * 0.5f + 0.5f) * 255);
            p[2] = (uint8_t)((nz * 0.5f + 0.5f) * 255);
            p[3] = 255;
        }
    return px;
}

WGPUBuffer make_buffer(WGPUDevice device, WGPUQueue queue, WGPUBufferUsage usage,
                       const void* data, size_t size) {
    WGPUBufferDescriptor bd = {};
    bd.usage = usage | WGPUBufferUsage_CopyDst;
    bd.size = size;
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &bd);
    wgpuQueueWriteBuffer(queue, buf, 0, data, size);
    return buf;
}

} // namespace

void Sprite::init(WGPUDevice device, WGPUQueue queue) {
    albedo_tex = upload_rgba8(device, queue, TEX, TEX, gen_albedo(), &albedo);
    normal_tex = upload_rgba8(device, queue, TEX, TEX, gen_normal(), &normal);

    WGPUSamplerDescriptor sd = {};
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sd.lodMinClamp = 0.0f;
    sd.lodMaxClamp = 1.0f;
    sd.maxAnisotropy = 1;
    sampler = wgpuDeviceCreateSampler(device, &sd);

    // pos.xy (unit-quad), uv — 4 вершины.
    const float verts[] = {
        -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 1.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f,
    };
    const uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    vbo = make_buffer(device, queue, WGPUBufferUsage_Vertex, verts, sizeof(verts));
    ibo = make_buffer(device, queue, WGPUBufferUsage_Index, idx, sizeof(idx));
}

void Sprite::shutdown() {
    if (vbo) wgpuBufferRelease(vbo);
    if (ibo) wgpuBufferRelease(ibo);
    if (sampler) wgpuSamplerRelease(sampler);
    if (albedo) wgpuTextureViewRelease(albedo);
    if (albedo_tex) wgpuTextureRelease(albedo_tex);
    if (normal) wgpuTextureViewRelease(normal);
    if (normal_tex) wgpuTextureRelease(normal_tex);
}
