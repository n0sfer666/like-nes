#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "graphics_sprite.hpp"
#include "quad_batch.hpp"
#include "rumble_level.hpp"

namespace rumble {

struct LayerStats {
    uint32_t zoom = 0;
    uint32_t sprites = 0;
    uint32_t unknown = 0;
    uint32_t quads = 0;
    uint32_t runs = 0;
    uint32_t rejected = 0;
    uint32_t dropped = 0;
};

// Кадр слоёв уровня в экранных пикселях: камера сама едет вдоль карты туда-обратно по тику, зум —
// наибольший целый, при котором карта влезает по высоте. Один и тот же кадр печатает headless-сводку
// и уходит в окно — сводка судит ровно то, что видит игрок.
class Layers {
public:
    static constexpr uint32_t CAPACITY = 4096;

    Layers();
    LayerStats build(const Level& level, uint32_t screen_w, uint32_t screen_h, uint64_t tick);

    std::span<const render::Quad> quads(const LayerStats& st) const {
        return {quads_.data(), st.quads};
    }
    std::span<const render::QuadRun> runs(const LayerStats& st) const {
        return {runs_.data(), st.runs};
    }

private:
    std::vector<framework::graphics::Sprite> sprites_;
    std::vector<uint64_t> keys_;
    std::vector<framework::graphics::Batch> batches_;
    std::vector<render::Quad> quads_;
    std::vector<render::QuadRun> runs_;
};

} // namespace rumble
