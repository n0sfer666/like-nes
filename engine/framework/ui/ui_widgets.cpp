#include "ui_context.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace framework::ui {
bool Context::button(UiId id, std::string_view key, UiRect rect, bool enabled) {
    if (!building_) return false;
    const auto value = label(key);
    auto* widget = register_widget(id, rect, enabled && !value.empty());
    if (!widget) return false;
    const uint32_t dropped_before = stats_.dropped_quads;
    panel(id, rect, widget->enabled);
    const int32_t pad = static_cast<int32_t>(theme_.padding);
    text(value, {rect.x + pad, rect.y + pad, rect.w - 2 * pad, rect.h - 2 * pad}, widget->enabled ? theme_.text : theme_.muted);
    return finish_widget(*widget, dropped_before) && activated(id);
}
bool Context::field(UiId id, std::string_view key, UiRect rect, std::span<char> buffer, bool enabled) {
    if (!building_) return false;
    const auto value = label(key);
    const auto* end = buffer.empty() || !buffer.data() ? nullptr :
        static_cast<const char*>(std::memchr(buffer.data(), 0, std::min(buffer.size(), UI_FIELD_BYTES)));
    if (!end) ++stats_.invalid_widgets;
    auto* widget = register_widget(id, rect, enabled && end && !value.empty());
    if (!widget) return false;
    const uint32_t dropped_before = stats_.dropped_quads;
    panel(id, rect, widget->enabled);
    const int32_t pad = static_cast<int32_t>(theme_.padding), split = rect.w / 2;
    const auto color = widget->enabled ? theme_.text : theme_.muted;
    text(value, {rect.x + pad, rect.y + pad, split - pad, rect.h - 2 * pad}, color);
    if (end) text({buffer.data(), static_cast<size_t>(end - buffer.data())},
                  {rect.x + split, rect.y + pad, rect.w - split - pad, rect.h - 2 * pad}, color);
    return finish_widget(*widget, dropped_before) && activated(id);
}
bool Context::slider(UiId id, std::string_view key, UiRect rect, float& value, float minimum,
                     float maximum, float step, bool enabled) {
    if (!building_) return false;
    const auto caption = label(key);
    const bool valid = std::isfinite(value) && std::isfinite(minimum) && std::isfinite(maximum) &&
                       std::isfinite(step) && minimum < maximum && step > 0 &&
                       std::isfinite(maximum - minimum);
    if (!valid) ++stats_.invalid_widgets;
    auto* widget = register_widget(id, rect, enabled && valid && !caption.empty());
    if (!widget) return false;
    const uint32_t dropped_before = stats_.dropped_quads;
    const float before = value;
    const int32_t pad = static_cast<int32_t>(theme_.padding);
    const auto bar = intersection({rect.x + pad, rect.y + rect.h - pad - 3, rect.w - 2 * pad, 3}, rect);
    if (bar.w <= 0 || bar.h <= 0) { widget->enabled = false; ++stats_.invalid_widgets; }
    if (widget->enabled) {
        if (!input_.cancelled && was_visible(id) && focus_ == id && input_.x)
            value = static_cast<float>(std::clamp(double(std::clamp(value, minimum, maximum)) + double(input_.x) * step,
                                                   double(minimum), double(maximum)));
        float x;
        if (dragging(id, x)) {
            const double fraction = std::clamp((double(x) - bar.x) / bar.w, 0.0, 1.0);
            value = static_cast<float>(double(minimum) + fraction * (double(maximum) - minimum));
        }
    }
    panel(id, rect, widget->enabled);
    text(caption, {rect.x + pad, rect.y + pad, rect.w - 2 * pad, rect.h - 2 * pad - 4}, widget->enabled ? theme_.text : theme_.muted);
    if (valid && bar.w > 0 && bar.h > 0) {
        fill(bar, theme_.disabled);
        const auto t = (std::clamp(value, minimum, maximum) - minimum) / (maximum - minimum);
        fill({bar.x, bar.y, static_cast<int32_t>(t * static_cast<float>(bar.w)), bar.h}, theme_.accent);
    }
    if (stats_.dropped_quads != dropped_before) value = before;
    return finish_widget(*widget, dropped_before) && value != before;
}
std::optional<uint32_t> Context::list(UiId parent, std::span<const UiListItem> items, UiRect rect, uint32_t row_height) {
    if (!building_) return {};
    if (!valid_rect(rect) || row_height == 0 || row_height > 32767) { ++stats_.invalid_widgets; return {}; }
    const auto rows = std::min({static_cast<size_t>(UI_WIDGETS), items.size(), (static_cast<size_t>(rect.h) + row_height - 1) / row_height});
    const auto dropped = items.size() - rows;
    stats_.dropped_widgets += static_cast<uint32_t>(std::min<size_t>(dropped, UINT32_MAX - stats_.dropped_widgets));
    std::optional<uint32_t> selected;
    for (uint32_t i = 0; i < rows; ++i) {
        const int32_t dy = static_cast<int32_t>(i * row_height);
        const auto& item = items[i];
        if (!string_key_ok(item.key)) { ++stats_.invalid_widgets; continue; }
        if (button(ui_id(parent, item.key, i), item.label_key,
                   {rect.x, rect.y + dy, rect.w, std::min(static_cast<int32_t>(row_height), rect.h - dy)}, item.enabled)) selected = i;
    }
    return selected;
}
}
