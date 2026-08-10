#include "cache.hpp"

namespace framework::physics {
namespace {

bool before(const Manifold& m, uint32_t key_a, uint32_t key_b) {
    return m.key_a < key_a || (m.key_a == key_a && m.key_b < key_b);
}

} // namespace

void ManifoldCache::reserve(size_t pairs) {
    previous_.reserve(pairs);
    resting_.reserve(pairs);
}

size_t ManifoldCache::find(uint32_t key_a, uint32_t key_b) const {
    size_t lo = 0;
    size_t hi = previous_.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (before(previous_[mid], key_a, key_b)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo >= previous_.size()) return previous_.size();
    const Manifold& prev = previous_[lo];
    if (prev.key_a != key_a || prev.key_b != key_b) return previous_.size();
    return lo;
}

void ManifoldCache::carry(Manifold& m) const {
    const size_t at = find(m.key_a, m.key_b);
    if (at == previous_.size()) return;
    const Manifold& prev = previous_[at];

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

bool ManifoldCache::recall(uint32_t key_a, uint32_t key_b, Manifold& out) const {
    const size_t at = find(key_a, key_b);
    // Тёплый старт (`carry`) флаг не спрашивает намеренно: ему нужен накопленный импульс, а тот
    // верен независимо от того, чьи позиции описывает геометрия записи.
    if (at == previous_.size() || resting_[at] == 0) return false;
    out = previous_[at];
    return true;
}

void ManifoldCache::store(const std::vector<Manifold>& contacts,
                          const std::vector<Manifold>& resting) {
    previous_.clear();
    resting_.clear();
    const auto take = [&](const Manifold& m, uint8_t was_resting) {
        previous_.push_back(m);
        resting_.push_back(was_resting);
    };
    size_t i = 0;
    size_t j = 0;
    while (i < contacts.size() && j < resting.size()) {
        if (before(contacts[i], resting[j].key_a, resting[j].key_b)) {
            take(contacts[i++], 0);
        } else {
            take(resting[j++], 1);
        }
    }
    while (i < contacts.size()) take(contacts[i++], 0);
    while (j < resting.size()) take(resting[j++], 1);
}

} // namespace framework::physics
