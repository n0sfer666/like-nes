#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>

#include "../light/table.hpp"

// Проход освещения графа кадра (гейт 7 спеки #18, вертикаль 3): albedo + нормали + ТАБЛИЦА
// источников -> освещённый кадр. Проход отделён от кадра, потому что его обязано быть можно
// ВЫКЛЮЧИТЬ целиком: выключенный он не создаёт ни текстуры, ни пайплайна, и кадр возвращается
// таким же, каким был до его появления, — это половина гейта.
namespace lightgfx {

// Раскладка источника на GPU: три vec4 подряд, std430-совместимо. Пинится static_assert'ом —
// расхождение с WGSL-структурой не диагностируется ничем, оно просто светит не туда.
struct GpuLight {
    float pos_h[4];
    float color_i[4];
    float dir_k[4];
};
static_assert(sizeof(GpuLight) == 48, "GpuLight layout pinned (matches WGSL struct Light)");

struct FrameUniform {
    float ambient[4];
    float view[4];
};
static_assert(sizeof(FrameUniform) == 32, "FrameUniform layout pinned (matches WGSL struct Frame)");

class Pass {
public:
    // Источники берутся у таблицы ЦЕЛИКОМ и здесь же уезжают в storage-буфер: их число — свойство
    // данных, а не кода, поэтому ни в шейдере, ни в этом классе константы «сколько» нет.
    bool init(WGPUDevice device, WGPUQueue queue, const light::Table& table,
              WGPUTextureFormat fmt, float aspect);
    void shutdown();

    // Рисует в `dst` из `albedo`, `normal` и `occlusion`. Любой из двух последних может быть
    // nullptr: тогда берётся собственная текстура 1x1 — плоская нормаль (0,0,1) либо «ничего не
    // перекрывает» (0). Это не запасной путь, а ручки гейта: кадр с картами и кадр без них обязаны
    // различаться, иначе «пришло из слота материала» неотличимо от «привязали одно и то же всем».
    void run(WGPUCommandEncoder enc, WGPUTextureView dst, WGPUTextureView albedo,
             WGPUTextureView normal, WGPUTextureView occlusion);

    uint32_t lights() const { return lights_; }

private:
    WGPUDevice device_ = nullptr;
    WGPUBindGroupLayout bgl_ = nullptr;
    WGPURenderPipeline pipe_ = nullptr;
    WGPUBuffer lights_ssbo_ = nullptr;
    WGPUBuffer frame_ubo_ = nullptr;
    WGPUSampler sampler_ = nullptr;
    WGPUTexture flat_normal_ = nullptr;
    WGPUTextureView flat_normal_view_ = nullptr;
    WGPUTexture open_occ_ = nullptr;
    WGPUTextureView open_occ_view_ = nullptr;
    uint32_t lights_ = 0;
};

} // namespace lightgfx
