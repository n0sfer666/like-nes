#pragma once
#include <array>

#include "rumble_layers.hpp"
#include "text_quads.hpp"
#include "touch_source.hpp"

namespace rumble {

// Тач рисуется в точках × плотность экрана, независимо от целого масштаба игрового viewport.
class TouchQuads {
public:
    void add(const framework::touch::TouchSource& source, const framework::graphics::FontView& font,
             float scale, Layers& layers, LayerStats& st, uint32_t solid_texture, uint32_t font_texture);

private:
    void disk(const framework::touch::Circle& circle, float scale, uint32_t rgba,
              Layers& layers, LayerStats& st, uint32_t texture);
    std::array<render::Quad, 16> quads_{};
    std::array<framework::graphics::GlyphPlace, 16> glyphs_{};
};

} // namespace rumble
