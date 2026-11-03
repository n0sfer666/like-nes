#include "ui_context.hpp"

#include <algorithm>

#include "ui_edit.hpp"

namespace framework::ui {
UiGridResult Context::glyph_grid(UiId id, std::string_view key, std::span<char> buffer,
                                 uint32_t max_len, UiRect rect, uint32_t columns, UiGridLabels labels) {
    UiGridResult result;
    if (!building_) { result.invalid = true; return result; }
    edit::Alphabet alphabet;
    edit::Buffer value;
    const auto chars = label(key), erase_label = label(labels.erase), done_label = label(labels.done);
    const bool valid = valid_rect(rect) && columns > 0 && columns <= UI_WIDGETS && max_len < UI_FIELD_BYTES &&
        alphabet.open(chars) && value.open(buffer) && !erase_label.empty() && !done_label.empty();
    if (!valid) { ++stats_.invalid_widgets; result.invalid = true; return result; }
    const uint32_t cells = alphabet.size + 2, rows = (cells + columns - 1) / columns;
    const int32_t cell_w = rect.w / static_cast<int32_t>(columns), cell_h = rect.h / static_cast<int32_t>(rows);
    // Не интерактивная обрезанная/частичная сетка: пользователь должен видеть все её ячейки.
    if (rect.x < 0 || rect.y < 0 || rect.x + rect.w > static_cast<int32_t>(view_.width) ||
        rect.y + rect.h > static_cast<int32_t>(view_.height) ||
        cell_w <= static_cast<int32_t>(2 * theme_.padding) || cell_h < font_.row->line_height + static_cast<int32_t>(2 * theme_.padding) ||
        cells > UI_WIDGETS - current_count_) {
        ++stats_.invalid_widgets; result.invalid = true; return result;
    }
    std::array<UiId, UI_WIDGETS> ids;
    for (uint32_t i = 0; i < current_count_; ++i)
        if (current_[i].columns && current_[i].group == id) { ++stats_.duplicate_ids; result.invalid = true; return result; }
    for (uint32_t i = 0; i < cells; ++i) {
        // Кодпоинт вместо позиции сохраняет идентичность при смене языка/порядка алфавита.
        ids[i] = ui_id(id, key, i < alphabet.size ? alphabet.codes[i] : UINT32_MAX - (i - alphabet.size));
        for (uint32_t j = 0; j < current_count_; ++j)
            if (current_[j].id == ids[i]) { ++stats_.duplicate_ids; result.invalid = true; return result; }
        for (uint32_t j = 0; j < i; ++j)
            if (ids[j] == ids[i]) { ++stats_.duplicate_ids; result.invalid = true; return result; }
    }
    const uint32_t first = current_count_, dropped_before = stats_.dropped_quads;
    std::optional<uint32_t> selected;
    bool complete = true;
    for (uint32_t i = 0; i < cells; ++i) {
        const UiRect cell{rect.x + static_cast<int32_t>(i % columns) * cell_w,
                          rect.y + static_cast<int32_t>(i / columns) * cell_h, cell_w, cell_h};
        auto* w = register_widget(ids[i], cell, true);
        if (!w) { complete = false; continue; }
        w->group = id; w->index = i; w->cells = cells; w->columns = columns;
        panel(w->id, cell, true);
        std::array<char, 4> encoded;
        const auto text_value = i < alphabet.size ? std::string_view(encoded.data(), edit::encode(alphabet.codes[i], encoded)) :
                                i == alphabet.size ? erase_label : done_label;
        const int32_t pad = static_cast<int32_t>(theme_.padding);
        text(text_value, {cell.x + pad, cell.y + pad, cell.w - 2 * pad, cell.h - 2 * pad}, theme_.text);
        complete = finish_widget(*w, dropped_before) && complete;
        if (activated(w->id)) selected = i;
    }
    if (!complete) {
        for (uint32_t i = first; i < current_count_; ++i) current_[i].enabled = false;
        result.invalid = true; return result;
    }
    bool focused = false;
    for (uint32_t i = first; i < current_count_; ++i) {
        if (focus_ != current_[i].id) continue;
        for (uint32_t j = 0; j < previous_count_; ++j) {
            const auto& old = previous_[j];
            if (old.id == current_[i].id && old.enabled && old.columns && old.group == id && old.panel_quad < UI_QUADS) focused = true;
        }
    }
    if (!focused || input_.cancelled) return result;
    if (input_.back && !back_consumed_) { result.back = true; back_consumed_ = true; return result; }
    if (input_.done || selected == alphabet.size + 1) { result.done = true; return result; }
    if (input_.erase || selected == alphabet.size) result.changed = value.erase();
    auto append = [&](uint32_t cp) {
        if (!alphabet.contains(cp)) { ++result.rejected; return; }
        if (value.append(cp, max_len)) result.changed = true;
        else result.full = true;
    };
    for (uint32_t i = 0; i < input_.character_count; ++i) append(input_.characters[i]);
    if (selected && *selected < alphabet.size) append(alphabet.codes[*selected]);
    return result;
}
}
