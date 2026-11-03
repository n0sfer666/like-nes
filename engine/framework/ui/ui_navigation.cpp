#include "ui_context.hpp"

#include <algorithm>

namespace framework::ui {
void Context::navigate() {
    if (!previous_count_) return;
    uint32_t at = 0;
    for (uint32_t i = 0; i < previous_count_; ++i) if (focus_ == previous_[i].id) at = i;
    const auto& old = previous_[at];
    if (old.columns && old.enabled && focus_ == old.id) {
        const uint32_t row = old.index / old.columns, col = old.index % old.columns;
        const auto next_row = std::clamp<int32_t>(static_cast<int32_t>(row) + (input_.y > 0) - (input_.y < 0),
                                                0, static_cast<int32_t>((old.cells - 1) / old.columns));
        const uint32_t row_begin = static_cast<uint32_t>(next_row) * old.columns;
        const uint32_t width = std::min(old.columns, old.cells - row_begin);
        const auto next_col = std::clamp<int32_t>(static_cast<int32_t>(col) + (input_.x > 0) - (input_.x < 0),
                                                0, static_cast<int32_t>(width - 1));
        const uint32_t target = row_begin + static_cast<uint32_t>(next_col);
        for (uint32_t i = 0; i < previous_count_; ++i) {
            const auto& w = previous_[i];
            if (w.enabled && w.columns && w.group == old.group && w.index == target) { focus_ = w.id; break; }
        }
        return;
    }
    if (!input_.y) return;
    for (uint32_t tries = 0; tries < previous_count_; ++tries) {
        at = input_.y > 0 ? (at + 1) % previous_count_ : (at + previous_count_ - 1) % previous_count_;
        if (previous_[at].enabled) { focus_ = previous_[at].id; break; }
    }
}
}
