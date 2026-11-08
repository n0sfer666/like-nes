#pragma once
#include "rumble_net_mode.hpp"
#include "rumble_screen.hpp"
#include "text_layout.hpp"

namespace rumble {

class NetworkQuads {
public:
    void add(const NetworkMode& network, const Credits& credits, const Play* play,
             uint32_t width, uint32_t height, uint32_t scale, Layers& layers, LayerStats& st);

private:
    std::array<framework::graphics::GlyphPlace, 160> places_{};
    std::array<render::Quad, 160> quads_{};
};

} // namespace rumble
