#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>
#include <vector>

#include "art.hpp"
#include "instance.hpp"

namespace game {

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
