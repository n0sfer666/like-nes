#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "lobby.hpp"
#include "quad_batch.hpp"
#include "rumble_credits.hpp"
#include "rumble_layers.hpp"
#include "text_layout.hpp"

namespace rumble {

// Потерянный пад важнее свободного места: в паузе драка стоит, и вход второго её не снимет.
// Пустая строка — все на местах, надписи нет.
std::string banner_text(const framework::input::Lobby& lobby);

// Надпись лобби строкой шрифта титров по центру низа ЗОНЫ, на подложке сплошной текстуры.
class BannerQuads {
public:
    static constexpr uint32_t CAPACITY = 64;

    BannerQuads();
    void add(const Credits& credits, std::string_view text, const framework::graphics::ViewportFit& fit,
             Layers& layers, LayerStats& st, uint32_t font_texture, uint32_t solid_texture);

private:
    std::vector<framework::graphics::GlyphPlace> places_;
    std::vector<render::Quad> quads_;
};

} // namespace rumble
