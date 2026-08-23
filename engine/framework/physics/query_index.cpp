#include "query_index.hpp"

#include <algorithm>

#include "sweep_order.hpp"
#include "units.hpp"

namespace framework::physics {

void QueryIndex::reserve(uint32_t capacity) {
    order_.reserve(capacity);
    bounds_.reserve(capacity);
    prefix_max_x_.reserve(capacity);
}

void QueryIndex::refresh(const std::vector<Body>& bodies) const {
    if (!dirty_) return;
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    order_.clear();
    bounds_.clear();
    prefix_max_x_.clear();
    for (uint32_t i = 0; i < n; ++i) {
        order_.push_back(i);
        // Расширяется ОДНА сторона из двух, вторую делает над своей коробкой сам запрос
        // (`query.cpp`), и половинного расширения не хватает: широкая фаза расширяет ОБЕ, и запас в
        // ней ровно вдвое больше допуска, с которым узкая фаза принимает касание. Отступление от
        // этой симметрии измерено — см. комментарий в `query.cpp`.
        bounds_.push_back(padded(bounds(bodies[i]), SPECULATIVE_MARGIN));
    }

    // Компаратор — ПОЛНЫЙ порядок из `sweep_order.hpp`, общий с широкой фазой шага. Здесь он стоит
    // дороже, чем там: порядок задаёт границы полосы, и разъехавшаяся полоса — это разный НАБОР
    // кандидатов на трёх ОС.
    const auto& bnd = bounds_;
    std::sort(order_.begin(), order_.end(), [&](uint32_t l, uint32_t r) {
        return sweep_before(bnd[l], bodies[l].key, bnd[r], bodies[r].key);
    });

    for (uint32_t i = 0; i < n; ++i) {
        const fix32 right = bounds_[order_[i]].max.x;
        prefix_max_x_.push_back(i == 0 || prefix_max_x_[i - 1] < right ? right : prefix_max_x_[i - 1]);
    }
    dirty_ = false;
}

uint32_t QueryIndex::band_begin(fix32 min_x) const {
    // Первая позиция, чей префиксный максимум достаёт до левого края запроса. Всё, что левее её,
    // заканчивается раньше запроса — целиком, а не по одному телу: в том и смысл префикса.
    uint32_t lo = 0;
    uint32_t hi = static_cast<uint32_t>(prefix_max_x_.size());
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (prefix_max_x_[mid] < min_x) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

uint32_t QueryIndex::band_end(fix32 max_x) const {
    // Первая позиция, чей ЛЕВЫЙ край лежит строго правее запроса. Список отсортирован по левому
    // краю, поэтому она и всё за ней запроса не касаются.
    uint32_t lo = 0;
    uint32_t hi = static_cast<uint32_t>(order_.size());
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (max_x < bounds_[order_[mid]].min.x) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

Band QueryIndex::band(const std::vector<Body>& bodies, const Aabb& probe) const {
    refresh(bodies);
    return {band_begin(probe.min.x), band_end(probe.max.x)};
}

} // namespace framework::physics
