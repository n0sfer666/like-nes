#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>
#include <memory>

#include "art.hpp"
#include "instance.hpp"
#include "instance_stage.hpp"
#include "material_fx.hpp"
#include "material_runs.hpp"

namespace game {

class SpriteBatch {
public:
    // `fx` может быть null или неготовым — тогда образец рисует как рисовал, без библиотеки.
    void init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target, const Atlas& atlas,
              MaterialFx* fx = nullptr);
    void set_viewport(uint32_t w, uint32_t h);
    void begin();
    void push(const Instance& inst);
    void push(const Instance& inst, uint32_t material);
    void flush(WGPURenderPassEncoder pass);
    void shutdown();

    // Вызовов отрисовки в последнем `flush`. Наружу — ради гейта: группировка прогонов проверяется
    // только числом вызовов, из картинки она не видна.
    uint32_t draws() const { return draws_; }

    // Сколько инстансов не влезло в кадр. Наружу — потому что отказ, который никто не спрашивает,
    // неотличим от кадра, где рисовать было нечего.
    uint32_t dropped() const { return stage_.dropped(); }

private:
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;
    WGPUTexture tex_ = nullptr;
    WGPUTextureView view_ = nullptr;
    WGPUSampler sampler_ = nullptr;
    WGPUBuffer quad_vbo_ = nullptr;
    WGPUBuffer quad_ibo_ = nullptr;
    WGPUBuffer inst_vbo_ = nullptr;
    WGPUBuffer vp_ubo_ = nullptr;
    WGPUBindGroupLayout bgl_ = nullptr;
    WGPUBindGroup bg_ = nullptr;
    WGPURenderPipeline pipe_ = nullptr;
    // Буфер кадра выделяется РОВНО ОДИН РАЗ, в `init`, и на стек не кладётся: сто килобайт в
    // кадровом кадре `run()` — это 10% стека главного потока на Windows, а `SpriteBatch` живёт всю
    // программу. Накопитель поверх него — `InstanceStage`, отдельным типом ради headless-гейта.
    std::unique_ptr<Instance[]> cpu_;
    InstanceStage stage_{nullptr, 0};
    std::unique_ptr<uint16_t[]> mat_;
    std::unique_ptr<MaterialRun[]> runs_;
    MaterialFx* fx_ = nullptr;
    uint32_t draws_ = 0;
    // Раскладка своя только без библиотеки: чужую освобождать нельзя, её владелец — кэш.
    bool owns_bgl_ = false;
};

// Проход с очисткой. Живёт здесь, а не в `draw.cpp`, потому что вызывающих ДВА образца, а
// `draw.hpp` тянет за собой flecs и частицы шутера — платформеру не нужные. Копий было две, и
// отличались они одним числом: цвет поэтому параметр.
WGPURenderPassEncoder begin_clear(WGPUCommandEncoder enc, WGPUTextureView view, WGPUColor clear);

} // namespace game
