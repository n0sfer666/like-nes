#pragma once
#include <webgpu/webgpu.h>

// Процедурный спрайт: albedo + normal-map (bevel) + общая quad-геометрия и сэмплер.
// Ассет-файлов нет — текстуры генерируются на CPU (детерминированно, самодостаточно).
struct Sprite {
    WGPUTexture albedo_tex = nullptr;
    WGPUTextureView albedo = nullptr;
    WGPUTexture normal_tex = nullptr;
    WGPUTextureView normal = nullptr;
    WGPUSampler sampler = nullptr;

    // Общая на все спрайты quad-геометрия (unit-quad [-0.5..0.5], uv 0..1).
    WGPUBuffer vbo = nullptr;
    WGPUBuffer ibo = nullptr;

    void init(WGPUDevice device, WGPUQueue queue);
    void shutdown();
};

constexpr uint32_t SPRITE_INDEX_COUNT = 6;
