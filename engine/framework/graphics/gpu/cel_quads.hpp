#pragma once
#include <cstdint>
#include <span>

#include "clip_debug.hpp"
#include "layer_quads.hpp"
#include "quad_batch.hpp"

// Шов кадра клипа к GPU (спека #24, В6б): клетка листа — один квад, оверлей `DebugDraw` — квады
// на сплошной текстуре 1×1. Квад вне листа, с зумом меньше 1 или с поворотом остаётся нулевым и
// считается.
namespace framework::graphics {

bool cel_quad(const ClipCel& cel, const CelPlace& at, TextureSize sheet, render::Quad& out);

struct DebugQuadStats {
    uint32_t quads = 0;
    uint32_t rejected = 0;
    uint32_t dropped = 0;
};

DebugQuadStats debug_quads(std::span<const DebugQuad> in, std::span<render::Quad> out);

} // namespace framework::graphics
