#pragma once
#include <webgpu/webgpu.h>

#include "arena.hpp"
#include "scene.hpp"
#include "sprite.hpp"

// Render-graph десктоп-tier: gbuffer(deferred) → lighting → forward → bloom → tonemap.
// Таргеты пре-варминг на init (арена, стабильные views), bind-group'ы собраны один раз →
// в render() НЕТ per-frame heap/GPU-аллокаций (инвариант #5).
class Renderer {
public:
    void init(WGPUDevice device, WGPUQueue queue, const Sprite& sprite,
              WGPUTextureFormat out_format, uint32_t w, uint32_t h);
    void render(const SceneSnapshot& snap, WGPUTextureView out_view);
    void shutdown();

    const TargetArena& arena() const { return arena_; }

private:
    void build_targets();
    void build_gbuffer();
    void build_lighting();
    void build_forward();
    void build_bloom();
    void build_tonemap();

    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;
    const Sprite* sprite_ = nullptr;
    WGPUTextureFormat out_format_ = WGPUTextureFormat_Undefined;
    uint32_t w_ = 0, h_ = 0;

    TargetArena arena_;
    WGPUTextureView albedo_ = nullptr;
    WGPUTextureView normal_ = nullptr;
    WGPUTextureView hdr_ = nullptr;
    WGPUTextureView bloom_a_ = nullptr;
    WGPUTextureView bloom_b_ = nullptr;
    uint32_t bloom_w_ = 0, bloom_h_ = 0;

    WGPUBuffer uniforms_ = nullptr;
    uint32_t uniform_stride_ = 256;
    WGPUBuffer lights_ubo_ = nullptr;
    WGPUBuffer blur_ubo_ = nullptr;

    WGPUBindGroupLayout gbuffer_bgl_ = nullptr;
    WGPUBindGroup gbuffer_bg_ = nullptr;
    WGPURenderPipeline gbuffer_pipe_ = nullptr;

    WGPUBindGroupLayout lighting_bgl_ = nullptr;
    WGPUBindGroup lighting_bg_ = nullptr;
    WGPURenderPipeline lighting_pipe_ = nullptr;

    WGPUBindGroupLayout forward_bgl_ = nullptr;
    WGPUBindGroup forward_bg_ = nullptr;
    WGPURenderPipeline forward_pipe_ = nullptr;

    WGPUBindGroupLayout bright_bgl_ = nullptr;
    WGPUBindGroup bright_bg_ = nullptr;
    WGPURenderPipeline bright_pipe_ = nullptr;

    WGPUBindGroupLayout blur_bgl_ = nullptr;
    WGPUBindGroup blur_bg_h_ = nullptr;
    WGPUBindGroup blur_bg_v_ = nullptr;
    WGPURenderPipeline blur_pipe_ = nullptr;

    WGPUBindGroupLayout tonemap_bgl_ = nullptr;
    WGPUBindGroup tonemap_bg_ = nullptr;
    WGPURenderPipeline tonemap_pipe_ = nullptr;
};

constexpr WGPUTextureFormat GBUFFER_ALBEDO_FMT = WGPUTextureFormat_RGBA8Unorm;
constexpr WGPUTextureFormat GBUFFER_NORMAL_FMT = WGPUTextureFormat_RGBA8Unorm;
constexpr WGPUTextureFormat HDR_FMT = WGPUTextureFormat_RGBA16Float;
