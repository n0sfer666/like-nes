#include "capture.hpp"

#include <webgpu/wgpu.h>

#include <cstring>

#include "stb_image_write.h"

namespace game {
namespace {

constexpr uint32_t align256(uint32_t v) { return (v + 255u) & ~255u; }

struct MapState { bool done = false; bool ok = false; };

} // namespace

std::vector<uint8_t> readback_rgba(WGPUDevice device, WGPUQueue queue, WGPUTexture tex,
                                   uint32_t w, uint32_t h) {
    const uint32_t row = w * 4;
    const uint32_t padded = align256(row);
    WGPUBufferDescriptor bd = {};
    bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bd.size = (uint64_t)padded * h;
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &bd);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUImageCopyTexture src = {};
    src.texture = tex; src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst = {};
    dst.buffer = buf;
    dst.layout.bytesPerRow = padded; dst.layout.rowsPerImage = h;
    WGPUExtent3D ext = {w, h, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &ext);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);

    MapState ms;
    wgpuBufferMapAsync(buf, WGPUMapMode_Read, 0, bd.size,
        [](WGPUBufferMapAsyncStatus status, void* ud) {
            auto* st = static_cast<MapState*>(ud);
            st->done = true;
            st->ok = status == WGPUBufferMapAsyncStatus_Success;
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
    return out;
}

bool write_png(const char* path, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
    return stbi_write_png(path, (int)w, (int)h, 4, rgba.data(), (int)(w * 4)) != 0;
}

} // namespace game
