#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "graphics_sprite.hpp"
#include "layer_draw.hpp"
#include "quad_batch.hpp"
#include "rumble_level.hpp"
#include "viewport_fit.hpp"

namespace rumble {

struct LayerStats {
    uint32_t scale = 0;
    uint32_t sprites = 0;
    uint32_t unknown = 0;
    uint32_t quads = 0;
    uint32_t runs = 0;
    uint32_t rejected = 0;
    uint32_t dropped = 0;
};

// Кадр слоёв уровня в экранных пикселях: камера идёт за точкой `follow`, а без неё сама ходит
// туда-обратно по отрезку центров, который оставляют `bounds` уровня при половине ЗОНЫ, масштаб и
// вид — из `viewport_fit`. Один и тот же кадр печатает headless-сводку и уходит в окно — сводка
// судит ровно то, что видит игрок.
class Layers {
public:
    static constexpr uint32_t CAPACITY = 4096;

    Layers();
    LayerStats build(const Level& level, const framework::graphics::ViewportFit& fit, uint64_t tick,
                     const framework::Vec2* follow = nullptr);

    std::span<const render::Quad> quads(const LayerStats& st) const {
        return {quads_.data(), st.quads};
    }
    std::span<const render::QuadRun> runs(const LayerStats& st) const {
        return {runs_.data(), st.runs};
    }
    const framework::graphics::LayerFrame& frame() const { return frame_; }
    void append(LayerStats& st, std::span<const render::Quad> extra, uint32_t texture);

private:
    std::vector<framework::graphics::Sprite> sprites_;
    std::vector<uint64_t> keys_;
    std::vector<framework::graphics::Batch> batches_;
    std::vector<render::Quad> quads_;
    std::vector<render::QuadRun> runs_;
    framework::graphics::LayerFrame frame_;
};

} // namespace rumble
