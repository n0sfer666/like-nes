#include "cache.hpp"

namespace framework::physics {

void ManifoldCache::reserve(size_t pairs) { previous_.reserve(pairs); }

void ManifoldCache::carry(Manifold& m) const {
    size_t lo = 0;
    size_t hi = previous_.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const Manifold& probe = previous_[mid];
        const bool before = probe.key_a < m.key_a || (probe.key_a == m.key_a && probe.key_b < m.key_b);
        if (before) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo >= previous_.size()) return;
    const Manifold& prev = previous_[lo];
    if (prev.key_a != m.key_a || prev.key_b != m.key_b) return;

    for (uint8_t i = 0; i < m.count; ++i) {
        for (uint8_t j = 0; j < prev.count; ++j) {
            if (prev.points[j].id != m.points[i].id) continue;
            m.points[i].normal_impulse = prev.points[j].normal_impulse;
            m.points[i].tangent_impulse = prev.points[j].tangent_impulse;
            // Переносится и `k`: подготовка читает его как ПРЕДЫДУЩИЙ, чтобы перевести накопленное
            // в новую шкалу, и только потом перезаписывает своим.
            m.points[i].normal.k = prev.points[j].normal.k;
            m.points[i].tangent.k = prev.points[j].tangent.k;
            break;
        }
    }
}

void ManifoldCache::store(const std::vector<Manifold>& manifolds) {
    previous_.clear();
    for (const Manifold& m : manifolds) previous_.push_back(m);
}

} // namespace framework::physics
