#include "capture.hpp"

#include "renderer.hpp"

#include <webgpu/wgpu.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stb_image.h"
#include "stb_image_write.h"

namespace {

constexpr uint32_t align256(uint32_t v) { return (v + 255u) & ~255u; }

struct MapState { bool done = false; bool ok = false; };

} // namespace

namespace capture {

std::vector<uint8_t> render_offscreen(WGPUDevice device, WGPUQueue queue, Renderer& renderer,
                                      const SceneSnapshot& snap, uint32_t w, uint32_t h) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{w, h, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1; td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);

    renderer.render(snap, view);

    const uint32_t row = w * 4;
    const uint32_t padded = align256(row);
    WGPUBufferDescriptor bd = {};
    bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bd.size = (uint64_t)padded * h;
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &bd);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUImageCopyTexture src = {};
    src.texture = tex; src.mipLevel = 0; src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst = {};
    dst.buffer = buf;
    dst.layout.offset = 0; dst.layout.bytesPerRow = padded; dst.layout.rowsPerImage = h;
    WGPUExtent3D ext = {w, h, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &ext);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);

    MapState ms;
    wgpuBufferMapAsync(buf, WGPUMapMode_Read, 0, bd.size,
        [](WGPUBufferMapAsyncStatus status, void* ud) {
            auto* s = static_cast<MapState*>(ud);
            s->done = true;
            s->ok = status == WGPUBufferMapAsyncStatus_Success;
        }, &ms);
    while (!ms.done) wgpuDevicePoll(device, true, nullptr);

    std::vector<uint8_t> out;
    if (ms.ok) {
        const uint8_t* mapped = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(buf, 0, bd.size));
        out.resize((size_t)row * h);
        for (uint32_t y = 0; y < h; ++y)
            std::memcpy(&out[(size_t)y * row], mapped + (size_t)y * padded, row);
        wgpuBufferUnmap(buf);
    }

    wgpuBufferRelease(buf);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(tex);
    return out;
}

bool write_png(const char* path, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
    return stbi_write_png(path, (int)w, (int)h, 4, rgba.data(), (int)(w * 4)) != 0;
}

bool read_png(const char* path, std::vector<uint8_t>& out, uint32_t& w, uint32_t& h) {
    int iw = 0, ih = 0, ch = 0;
    unsigned char* data = stbi_load(path, &iw, &ih, &ch, 4);
    if (!data) return false;
    w = (uint32_t)iw; h = (uint32_t)ih;
    out.assign(data, data + (size_t)iw * ih * 4);
    stbi_image_free(data);
    return true;
}

DiffResult compare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                   double per_pixel_eps, double frac_tolerance, double max_abs_cap) {
    DiffResult r = {0, 0, 0, false};
    if (a.size() != b.size() || a.empty() || a.size() % 4 != 0) return r;
    const size_t pixels = a.size() / 4;
    double sum = 0; size_t px_over = 0;
    for (size_t p = 0; p < pixels; ++p) {
        double pmax = 0;
        for (size_t c = 0; c < 4; ++c) {
            const size_t i = p * 4 + c;
            const double d = std::fabs((double)a[i] - (double)b[i]) / 255.0;
            sum += d;
            if (d > r.max_abs) r.max_abs = d;
            if (d > pmax) pmax = d;
        }
        if (pmax > per_pixel_eps) ++px_over;
    }
    r.mean_abs = sum / a.size();
    r.frac_over = (double)px_over / pixels;
    r.pass = r.frac_over <= frac_tolerance && r.max_abs <= max_abs_cap;
    return r;
}

} // namespace capture
