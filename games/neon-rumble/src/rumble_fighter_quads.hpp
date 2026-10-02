#pragma once
#include <cstdint>
#include <vector>

#include "debug_draw.hpp"
#include "quad_batch.hpp"
#include "rumble_fighter.hpp"
#include "rumble_layers.hpp"

namespace rumble {

struct FighterStats {
    uint32_t overlay = 0;
    uint32_t rejected = 0;
    uint32_t dropped = 0;
};

// Боец поверх слоёв уровня: пивот — спавн через ту же камеру, что у слоя объектов, клетка — квад
// листа `sheet_texture`, оверлей F3 — квады сплошной текстуры `sheet_texture + 1`.
class FighterQuads {
public:
    static constexpr uint32_t OVERLAY = 128;

    FighterQuads();
    FighterStats add(const Fighter& fighter, const Pose& pose, Layers& layers, LayerStats& st,
                     uint32_t sheet_texture, bool overlay);

private:
    std::vector<framework::graphics::DebugQuad> debug_;
    std::vector<render::Quad> quads_;
};

} // namespace rumble
