#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>
#include <vector>

#include "cache.hpp"
#include "instance.hpp"
#include "table.hpp"

// Сцена гейтов 4/5/6 спеки #18: каждый материал библиотеки нарисован НЕСКОЛЬКИМИ объектами с
// разными значениями параметров. Вопрос сцены не «красиво ли», а «сколько вышло вызовов
// отрисовки»: параметры едут в инстансном буфере, поэтому четыре объекта одного материала и семь
// материалов, делящих три точки входа, обязаны дать ТРИ вызова, а не двадцать восемь.
namespace matgold {

constexpr uint32_t PER_MATERIAL = 4;

struct Batch {
    WGPURenderPipeline pipe;
    uint32_t first;
    uint32_t count;
};

class Scene {
public:
    bool init(WGPUDevice device, WGPUQueue queue, uint32_t w, uint32_t h,
              WGPUBindGroupLayout bgl);
    void shutdown();

    // Инстансы из таблицы: значения берутся у `resolve`, а варьируемый параметр — у слота 4,
    // который у всех трёх эффектов свой (сила вспышки, толщина обводки, порог растворения).
    void build(const mat::Table& t, mat::Cache& cache);

    // Возвращает число вызовов отрисовки — то самое, что утверждает гейт 5.
    uint32_t draw(WGPURenderPassEncoder pass);

    uint32_t instances() const { return static_cast<uint32_t>(items_.size()); }

private:
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;
    WGPUBuffer quad_vbo_ = nullptr;
    WGPUBuffer quad_ibo_ = nullptr;
    WGPUBuffer inst_vbo_ = nullptr;
    WGPUBuffer vp_ubo_ = nullptr;
    WGPUTexture albedo_ = nullptr;
    WGPUTexture aux_ = nullptr;
    WGPUTextureView albedo_view_ = nullptr;
    WGPUTextureView aux_view_ = nullptr;
    WGPUSampler sampler_ = nullptr;
    WGPUBindGroup bg_ = nullptr;
    std::vector<mat::Instance> items_;
    std::vector<Batch> batches_;
};

} // namespace matgold
