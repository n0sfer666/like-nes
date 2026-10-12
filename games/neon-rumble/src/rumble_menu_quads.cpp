#include "rumble_menu_quads.hpp"

#include <string>

#include "text_quads.hpp"

namespace rumble {

namespace {

using namespace framework::graphics;

constexpr uint32_t ERROR_RGBA = 0xFF5C5CFFu;
constexpr uint32_t ITEM_RGBA = 0xFFFFFFFFu;
constexpr uint32_t PICK_RGBA = 0xFFE45CFFu;

} // namespace

MenuQuads::MenuQuads() : places_(CAPACITY), quads_(CAPACITY) {}

MenuStats MenuQuads::add(const Menu& menu, const Credits& credits, const ViewportFit& fit, Layers& layers,
                         LayerStats& st) {
    MenuStats ms;
    if (credits.font.row == nullptr) return ms;
    const uint32_t scale = fit.scale == 0 ? 1 : fit.scale;
    const auto h = static_cast<int32_t>(credits.font.row->line_height * scale);
    const uint32_t error_lines =
        menu.error.empty() ? 0 : layout_text(credits.font, menu.error, fit.zone.w / scale, places_).lines + 1;
    const auto rows = static_cast<int32_t>(MENU_ITEMS + error_lines);
    int32_t y = fit.zone.y + (static_cast<int32_t>(fit.zone.h) - rows * h) / 2;
    if (!menu.error.empty()) y += line(credits, menu.error, ERROR_RGBA, fit, y, layers, st, ms) + h;
    for (uint32_t i = 0; i < MENU_ITEMS; ++i) {
        const auto item = static_cast<MenuItem>(i);
        const bool picked = item == menu.item;
        const std::string text = (picked ? "> " : "  ") + menu.label(item);
        y += line(credits, text, picked ? PICK_RGBA : ITEM_RGBA, fit, y, layers, st, ms);
    }
    return ms;
}

int32_t MenuQuads::line(const Credits& credits, std::string_view text, uint32_t rgba, const ViewportFit& fit,
                        int32_t y, Layers& layers, LayerStats& st, MenuStats& ms) {
    const PixelRect& z = fit.zone;
    const uint32_t scale = fit.scale == 0 ? 1 : fit.scale;
    const TextStats t = layout_text(credits.font, text, z.w / scale, places_);
    const int32_t x = z.x + (static_cast<int32_t>(z.w) - static_cast<int32_t>(t.width * scale)) / 2;
    const TextQuadStats q = text_quads(credits.font, {places_.data(), t.placed}, TextPen{x, y, scale, rgba}, quads_);
    layers.append(st, {quads_.data(), q.quads}, MENU_FONT_TEXTURE);
    ms.unknown += t.unknown;
    ms.dropped += t.dropped + q.dropped;
    return static_cast<int32_t>(t.lines * credits.font.row->line_height * scale);
}

} // namespace rumble
