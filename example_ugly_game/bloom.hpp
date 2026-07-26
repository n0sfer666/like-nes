#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>

namespace game {

// Bloom-пост-процесс (S9, техника из спеки #2 render/shaders_post): игра рендерит в HDR-таргет
// (RGBA16Float) → bright-pass (порог яркости) → сепарабельный blur (half-res ping-pong) →
// ACES-tonemap + additive-композит → swapchain. Яркие спрайты (tint>1.0) светятся.
class Bloom {
public:
    bool init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat out_fmt, uint32_t w, uint32_t h);
    WGPUTextureView hdr_view() const { return hdr_view_; }   // игра рендерит СЮДА
    void resolve(WGPUCommandEncoder enc, WGPUTextureView out_view);
    void shutdown();

private:
    WGPUTexture hdr_ = nullptr, bloom_a_ = nullptr, bloom_b_ = nullptr;
    WGPUTextureView hdr_view_ = nullptr, bloom_a_v_ = nullptr, bloom_b_v_ = nullptr;
    WGPUSampler samp_ = nullptr;
    WGPUBuffer ubo_h_ = nullptr, ubo_v_ = nullptr;
    WGPUBindGroupLayout bright_bgl_ = nullptr, blur_bgl_ = nullptr, tone_bgl_ = nullptr;
    WGPUBindGroup bright_bg_ = nullptr, blur_bg_h_ = nullptr, blur_bg_v_ = nullptr, tone_bg_ = nullptr;
    WGPURenderPipeline bright_pipe_ = nullptr, blur_pipe_ = nullptr, tone_pipe_ = nullptr;
};

} // namespace game
