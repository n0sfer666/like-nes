#include "rumble_net_quads.hpp"

#include <algorithm>

#include "text_quads.hpp"

namespace rumble {

void NetworkQuads::add(const NetworkMode& network, const Credits& credits, const Play* play,
                       uint32_t width, uint32_t height, uint32_t scale, Layers& layers, LayerStats& st) {
    using namespace framework::graphics;
    if (!credits.font.row || !width || !height) return;
    scale = std::clamp(scale, 1u, 2u);
    const uint32_t font = play ? atlas_texture(play->level.level.texture_count) : MENU_FONT_TEXTURE;
    const uint32_t solid = play ? solid_texture(play->level.level.texture_count) : MENU_SOLID_TEXTURE;
    const auto panel = network.panel();
    const auto w = static_cast<float>(width), h = static_cast<float>(height);
    for (const auto& cell : std::span(panel.cells).first(panel.count)) {
        const auto max_width = static_cast<uint32_t>(cell.width * w / static_cast<float>(scale));
        if (!max_width) continue;
        const auto layout = layout_text(credits.font, cell.text, max_width, places_);
        const auto text_w = static_cast<int32_t>(layout.width * scale);
        const auto text_h = static_cast<int32_t>(credits.font.row->line_height * scale);
        const auto x = static_cast<int32_t>(cell.x * w) - text_w / 2;
        const auto y = static_cast<int32_t>(cell.y * h) - text_h / 2;
        if (cell.action || play) {
            render::Quad back;
            back.x = static_cast<float>(x - 6);
            back.y = static_cast<float>(y - 2);
            back.w = static_cast<float>(text_w + 12);
            back.h = static_cast<float>(text_h + 4);
            back.tw = back.th = 1;
            back.rgba = 0x17102deeu;
            layers.append(st, {&back, 1}, solid);
        }
        const uint32_t color = cell.selected ? 0xffe45cffu : 0xffffffffu;
        const auto q = text_quads(credits.font, {places_.data(), layout.placed}, TextPen{x, y, scale, color}, quads_);
        layers.append(st, {quads_.data(), q.quads}, font);
    }
}

} // namespace rumble
