#include "ui_context.hpp"

#include <algorithm>
#include <cmath>

#include "hash.hpp"

namespace framework::ui {
UiId ui_id(UiId parent, std::string_view key, uint32_t index) {
    auto hash = asset::fnv1a_u64(asset::FNV_OFFSET, parent);
    hash = asset::fnv1a_u64(hash, key.size());
    return asset::fnv1a_u32(asset::fnv1a(key.data(), key.size(), hash), index);
}
bool Context::valid_rect(UiRect r) {
    return r.x >= -32767 && r.x <= 32767 && r.y >= -32767 && r.y <= 32767 &&
           r.w > 0 && r.w <= 32767 && r.h > 0 && r.h <= 32767;
}
bool Context::contains(UiRect r, float x, float y) {
    return x >= static_cast<float>(r.x) && y >= static_cast<float>(r.y) &&
           x < static_cast<float>(r.x + r.w) && y < static_cast<float>(r.y + r.h);
}
UiRect Context::intersection(UiRect a, UiRect b) {
    const int32_t x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0, std::min(a.x + a.w, b.x + b.w) - x),
                  std::max(0, std::min(a.y + a.h, b.y + b.h) - y)};
}
bool Context::init(const Strings& strings, const graphics::FontTable& fonts, uint32_t font_texture,
                   uint32_t solid_texture, UiTheme theme) {
    ready_ = false; building_ = false; previous_count_ = current_count_ = quad_count_ = run_count_ = 0;
    focus_.reset(); click_.reset(); pointer_ = {}; stats_ = {};
    font_ = fonts.find(strings.font_name());
    if (!strings.count() || !font_.row || theme.padding > 32) return false;
    strings_ = strings; theme_ = theme; font_texture_ = font_texture; solid_texture_ = solid_texture;
    language_ = Language::En; ready_ = true;
    return true;
}
void Context::set_language(Language language) {
    if (language == Language::En || language == Language::Ru) language_ = language;
}
bool Context::begin(const UiInput& input, UiView view) {
    const bool valid_view = view.width > 0 && view.width <= 32767 && view.height > 0 && view.height <= 32767 &&
        view.scale > 0 && view.scale <= 64 && view.x >= -1048576 && view.x <= 1048576 && view.y >= -1048576 && view.y <= 1048576;
    if (!ready_ || building_ || !valid_view) {
        building_ = false; previous_count_ = current_count_ = quad_count_ = run_count_ = 0;
        focus_.reset(); pointer_ = {}; click_.reset(); return false;
    }
    stats_ = {}; current_count_ = quad_count_ = run_count_ = 0; consumed_ = back_consumed_ = false; click_.reset();
    input_ = input;
    if (input.pointer_count > UI_POINTERS || input.character_count > UI_CHARACTERS) input_.cancelled = true;
    for (uint32_t i = 0; i < std::min<uint32_t>(input.pointer_count, static_cast<uint32_t>(UI_POINTERS)); ++i) {
        const auto& p = input.pointers[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.phase > UiPointerPhase::Cancel) input_.cancelled = true;
    }
    if (previous_count_ && view != view_) input_.cancelled = true;
    view_ = view; pointer_.released = false;
    if (input_.cancelled) pointer_ = {};
    else {
        navigate();
        pointer_events();
    }
    building_ = true;
    return true;
}
void Context::pointer_events() {
    const UiRect zone{0, 0, static_cast<int32_t>(view_.width), static_cast<int32_t>(view_.height)};
    for (uint32_t i = 0; i < input_.pointer_count; ++i) {
        const auto& e = input_.pointers[i];
        if (e.phase == UiPointerPhase::Cancel && e.id == pointer_.id) { pointer_ = {}; click_.reset(); continue; }
        if (e.phase == UiPointerPhase::Down && !pointer_.held && !click_) {
            pointer_ = {e.id, {}, e.x, e.y, true, false};
            if (!contains(zone, e.x, e.y)) continue;
            for (uint32_t row = previous_count_; row > 0; --row) {
                const auto& w = previous_[row - 1];
                if (w.panel_quad == UI_QUADS || !contains(w.rect, e.x, e.y)) continue;
                if (w.enabled) { pointer_.target = w.id; focus_ = w.id; }
                break; // видимый disabled widget перекрывает нижние
            }
        } else if (pointer_.held && pointer_.id == e.id) {
            pointer_.x = e.x; pointer_.y = e.y;
            if (e.phase != UiPointerPhase::Up) continue;
            pointer_.held = false; pointer_.released = true;
            for (uint32_t row = 0; row < previous_count_; ++row) {
                const auto& w = previous_[row];
                if (pointer_.target == w.id && contains(zone, e.x, e.y) && contains(w.rect, e.x, e.y)) { click_ = w.id; break; }
            }
        }
    }
}
Context::Widget* Context::register_widget(UiId id, UiRect rect, bool enabled) {
    if (!building_) return nullptr;
    if (!valid_rect(rect)) { ++stats_.invalid_widgets; return nullptr; }
    for (uint32_t i = 0; i < current_count_; ++i)
        if (current_[i].id == id) { ++stats_.duplicate_ids; return nullptr; }
    if (current_count_ == UI_WIDGETS) { ++stats_.dropped_widgets; return nullptr; }
    const auto visible = intersection(rect, {0, 0, static_cast<int32_t>(view_.width), static_cast<int32_t>(view_.height)});
    enabled = enabled && visible.w > 0 && visible.h > 0;
    current_[current_count_++] = {id, rect, enabled}; ++stats_.widgets;
    if (!focus_ && enabled) focus_ = id;
    return &current_[current_count_ - 1];
}
bool Context::finish_widget(Widget& widget, uint32_t dropped_before) {
    if (stats_.dropped_quads != dropped_before) widget.enabled = false;
    return widget.enabled;
}
bool Context::was_visible(UiId id) const {
    for (uint32_t i = 0; i < previous_count_; ++i)
        if (previous_[i].id == id && previous_[i].enabled && previous_[i].panel_quad < UI_QUADS) return true;
    return false;
}
bool Context::activated(UiId id) {
    if (consumed_ || input_.cancelled || !was_visible(id)) return false;
    for (uint32_t i = 0; i < current_count_; ++i) if (current_[i].id == id && !current_[i].enabled) return false;
    if (click_ != id && !(focus_ == id && input_.accept)) return false;
    consumed_ = true;
    return true;
}
bool Context::dragging(UiId id, float& x) const {
    x = pointer_.x;
    return !input_.cancelled && pointer_.target == id && (pointer_.held || pointer_.released);
}
UiOutput Context::end() {
    if (!building_) return {};
    building_ = false;
    bool found = false;
    for (uint32_t i = 0; i < current_count_; ++i) if (focus_ == current_[i].id && current_[i].enabled) found = true;
    if (!found) {
        focus_.reset();
        for (uint32_t i = 0; i < current_count_; ++i) if (current_[i].enabled) { focus_ = current_[i].id; break; }
    }
    for (uint32_t i = 0; i < current_count_; ++i) {
        const auto& w = current_[i];
        if (w.panel_quad < quad_count_)
            quads_[w.panel_quad].rgba = w.enabled && focus_ == w.id ? theme_.focused : w.enabled ? theme_.normal : theme_.disabled;
    }
    if (pointer_.target) {
        bool retained = false;
        for (uint32_t i = 0; i < current_count_; ++i) if (pointer_.target == current_[i].id && current_[i].enabled) retained = true;
        if (!retained) pointer_ = {};
    }
    previous_count_ = current_count_;
    std::copy_n(current_.begin(), current_count_, previous_.begin());
    return {{quads_.data(), quad_count_}, {runs_.data(), run_count_}, stats_, focus_, !input_.cancelled && input_.back && !back_consumed_};
}
}
