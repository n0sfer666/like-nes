#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <span>
#include <vector>

// Квады слоёв и спрайтов из RGBA8-текстур (спека #24, В5б). Геометрия — в экранных пикселях от
// левого верхнего угла, источник — прямоугольник текселей; тексель берётся `textureLoad`, а не
// семплером: пиксельный голден сравнивается точно на Metal, lavapipe и WARP.
namespace render {

struct Quad {
    float x = 0, y = 0, w = 0, h = 0;
    float tx = 0, ty = 0, tw = 0, th = 0;
    uint32_t rgba = 0xffffffffu;
    uint32_t flip = 0;
};
static_assert(sizeof(Quad) == 40);

struct QuadRun {
    uint32_t first = 0;
    uint32_t count = 0;
    uint32_t texture = 0;
};

class QuadRenderer {
public:
    QuadRenderer() = default;
    QuadRenderer(const QuadRenderer&) = delete;
    QuadRenderer& operator=(const QuadRenderer&) = delete;
    ~QuadRenderer() { shutdown(); }

    bool init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target, uint32_t capacity);
    // Номер текстуры — порядок добавления; он же `material` спрайта.
    bool add_texture(const uint8_t* rgba, uint32_t w, uint32_t h);
    uint32_t texture_count() const { return static_cast<uint32_t>(textures_.size()); }
    bool upload(std::span<const Quad> quads, uint32_t screen_w, uint32_t screen_h);
    // Число draw-call; прогон за пределами загруженного или с чужой текстурой пропускается.
    uint32_t draw(WGPURenderPassEncoder pass, std::span<const QuadRun> runs) const;
    void shutdown();

private:
    struct Texture {
        WGPUTexture texture = nullptr;
        WGPUTextureView view = nullptr;
        WGPUBindGroup group = nullptr;
    };
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;
    WGPUBindGroupLayout layout_ = nullptr;
    WGPURenderPipeline pipeline_ = nullptr;
    WGPUBuffer instances_ = nullptr;
    WGPUBuffer screen_ = nullptr;
    WGPUTextureFormat texel_ = WGPUTextureFormat_RGBA8Unorm;
    uint32_t capacity_ = 0;
    uint32_t uploaded_ = 0;
    std::vector<Texture> textures_;
};

} // namespace render
