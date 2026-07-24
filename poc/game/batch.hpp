#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>
#include <vector>

#include "art.hpp"

namespace game {

struct Instance {
    float x, y, w, h;
    float u0, v0, u1, v1;
    float r, g, b, a;
    float rot = 0;   // S9: поворот квада (наклон корабля, вращение частиц)
};

constexpr uint32_t MAX_INSTANCES = 2048;   // S9: + частицы

class SpriteBatch {
public:
    void init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target, const Atlas& atlas);
    void set_viewport(uint32_t w, uint32_t h);
    void begin();
    void push(const Instance& inst);
    void flush(WGPURenderPassEncoder pass);
    void shutdown();

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
    std::vector<Instance> cpu_;
};

} // namespace game
