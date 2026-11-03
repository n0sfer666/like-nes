#include "ui_context.hpp"

#include <algorithm>

#include "text_layout.hpp"
#include "utf8_decode.hpp"

namespace framework::ui {
void Context::quad(render::Quad value, uint32_t texture) {
    const bool merge = run_count_ && runs_[run_count_ - 1].texture == texture;
    if (quad_count_ == UI_QUADS || (!merge && run_count_ == UI_RUNS)) { ++stats_.dropped_quads; return; }
    if (merge) ++runs_[run_count_ - 1].count;
    else runs_[run_count_++] = {quad_count_, 1, texture};
    quads_[quad_count_++] = value; ++stats_.quads;
}
void Context::fill(UiRect rect, uint32_t color) {
    const auto r = intersection(rect, {0, 0, static_cast<int32_t>(view_.width), static_cast<int32_t>(view_.height)});
    if (r.w <= 0 || r.h <= 0) return;
    const float scale = static_cast<float>(view_.scale);
    render::Quad q;
    q.x = static_cast<float>(view_.x) + static_cast<float>(r.x) * scale;
    q.y = static_cast<float>(view_.y) + static_cast<float>(r.y) * scale;
    q.w = static_cast<float>(r.w) * scale; q.h = static_cast<float>(r.h) * scale;
    q.tw = q.th = 1; q.rgba = color;
    quad(q, solid_texture_);
}
void Context::panel(UiId id, UiRect rect, bool enabled) {
    const uint32_t first = quad_count_;
    fill(rect, enabled && focus_ == id ? theme_.focused : enabled ? theme_.normal : theme_.disabled);
    if (first < quad_count_) for (uint32_t i = 0; i < current_count_; ++i)
        if (current_[i].id == id) current_[i].panel_quad = first;
    // Два слоя всегда: end может исправить подсветку после удаления прежнего focused widget.
    if (rect.w > 2 && rect.h > 2)
        fill({rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, enabled ? theme_.normal : theme_.disabled);
}
std::string_view Context::label(std::string_view key) {
    const auto value = strings_.find(key, language_);
    if (value.empty()) ++stats_.missing_labels;
    return value;
}
void Context::text(std::string_view value, UiRect rect, uint32_t color) {
    if (rect.w <= 0 || rect.h <= 0 || !font_.row) return;
    const auto clip = intersection(rect, {0, 0, static_cast<int32_t>(view_.width), static_cast<int32_t>(view_.height)});
    if (clip.w <= 0 || clip.h <= 0) return;
    // Не сканировать всю phrase16MiB при каждом кадре; UTF-8 обрезается только на границе символа.
    size_t at = 0; uint32_t cp = 0, count = 0;
    while (count < UI_TEXT_GLYPHS && core::utf8_next(value, at, cp)) ++count;
    if (at < value.size()) ++stats_.clipped_glyphs;
    value = value.substr(0, at);
    std::array<graphics::GlyphPlace, UI_TEXT_GLYPHS> places;
    const auto placed = graphics::layout_text(font_, value, static_cast<uint32_t>(rect.w), places);
    stats_.unknown_glyphs += placed.unknown;
    const float scale = static_cast<float>(view_.scale);
    for (uint32_t i = 0; i < placed.placed; ++i) {
        const auto& p = places[i];
        const UiRect glyph{rect.x + p.x, rect.y + p.y, p.glyph->w, font_.row->line_height};
        const auto visible = intersection(glyph, clip);
        if (visible.w != glyph.w || visible.h != glyph.h) ++stats_.clipped_glyphs;
        if (visible.w <= 0 || visible.h <= 0) continue;
        render::Quad q;
        q.x = static_cast<float>(view_.x) + static_cast<float>(visible.x) * scale;
        q.y = static_cast<float>(view_.y) + static_cast<float>(visible.y) * scale;
        q.w = static_cast<float>(visible.w) * scale; q.h = static_cast<float>(visible.h) * scale;
        q.tx = static_cast<float>(p.glyph->x + visible.x - glyph.x);
        q.ty = static_cast<float>(p.glyph->y + visible.y - glyph.y);
        q.tw = static_cast<float>(visible.w); q.th = static_cast<float>(visible.h); q.rgba = color;
        quad(q, font_texture_);
    }
}
}
