#include "rumble_touch_quads.hpp"

#include <algorithm>
#include <cmath>

namespace rumble {
namespace {

using framework::touch::Circle;
using framework::touch::Control;
using namespace framework::graphics;

float px(fix32 value, float scale) { return static_cast<float>(value.to_double()) * scale; }

constexpr uint32_t IDLE = 0xFFFFFF40u, HELD = 0xFFE45CA0u;
constexpr const char* LABELS[] = {"PUNCH", "KICK", "JUMP", "GRAB", "BLOCK"};

} // namespace

void TouchQuads::disk(const Circle& circle, float scale, uint32_t rgba, Layers& layers, LayerStats& st,
                       uint32_t texture) {
    constexpr int WIDTHS[] = {3, 5, 7, 8, 8, 7, 5, 3};
    const float r = px(circle.radius, scale) * 0.9f;
    const float x = px(circle.center.x, scale), y = px(circle.center.y, scale);
    for (uint32_t i = 0; i < 8; ++i) {
        render::Quad& q = quads_[i];
        q = {};
        q.w = static_cast<float>(WIDTHS[i]) * r / 4;
        q.h = r / 4;
        q.x = x - q.w / 2;
        q.y = y - r + static_cast<float>(i) * q.h;
        q.tw = q.th = 1;
        q.rgba = rgba;
    }
    layers.append(st, {quads_.data(), 8}, texture);
}

void TouchQuads::add(const framework::touch::TouchSource& source, const FontView& font, float scale,
                      Layers& layers, LayerStats& st, uint32_t solid_texture, uint32_t font_texture) {
    if (!std::isfinite(scale) || scale <= 0 || scale > 16) return;
    const uint32_t text_scale = std::max(1u, static_cast<uint32_t>(std::lround(scale)));
    for (uint32_t i = 0; i < framework::touch::BUTTONS; ++i) {
        const auto circle = source.button(i);
        if (!circle) continue;
        const bool held = source.owner(static_cast<Control>(i)).has_value();
        disk(*circle, scale, held ? HELD : IDLE, layers, st, solid_texture);
        if (font.row == nullptr) continue;
        const uint32_t width = static_cast<uint32_t>(px(circle->radius, 2 * scale)) / text_scale;
        const TextStats text = layout_text(font, LABELS[i], width, glyphs_);
        const int32_t x = static_cast<int32_t>(px(circle->center.x, scale)) -
            static_cast<int32_t>(text.width * text_scale) / 2;
        const int32_t y = static_cast<int32_t>(px(circle->center.y, scale)) -
            static_cast<int32_t>(font.row->line_height * text_scale) / 2;
        const TextQuadStats q = text_quads(font, {glyphs_.data(), text.placed},
            TextPen{x, y, text_scale, held ? 0x000000FFu : 0xFFFFFFC0u}, quads_);
        layers.append(st, {quads_.data(), q.quads}, font_texture);
        st.unknown += text.unknown;
        st.dropped += text.dropped + q.dropped;
    }
    if (const auto base = source.stick_base()) disk(*base, scale, IDLE, layers, st, solid_texture);
    if (const auto knob = source.stick_knob())
        disk(Circle{*knob, fix32::from_int(18)}, scale, HELD, layers, st, solid_texture);
}

} // namespace rumble
