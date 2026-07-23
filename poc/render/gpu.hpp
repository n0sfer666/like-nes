#pragma once
#include <webgpu/webgpu.h>

// Общий WebGPU-контекст для оконного (demo) и headless (golden) путей.
// surface может быть null — тогда адаптер запрашивается без compatibleSurface (offscreen/CI).
struct GpuContext {
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    bool supports_bc = false; // device включил TextureCompressionBC (baked BC7-шов доступен)

    bool init(WGPUSurface surface);
    void shutdown();
};
