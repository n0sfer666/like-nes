#include "broadphase.hpp"

#include <algorithm>

namespace framework::physics {
namespace {

// Пара тел, ни одно из которых не может двигаться от удара, решателю не нужна: импульс, делённый
// на нулевую сумму обратных масс, всё равно был бы отброшен. Отсекается здесь, а не в решателе, —
// иначе пол из тысячи статических плиток дал бы тысячи пар за кадр на ровном месте.
bool both_immovable(const Body& a, const Body& b) {
    return a.type != BodyType::Dynamic && b.type != BodyType::Dynamic;
}

} // namespace

void Broadphase::reserve(uint32_t capacity) {
    order_.reserve(capacity);
    bounds_.reserve(capacity);
}

void Broadphase::build(const std::vector<Body>& bodies, std::vector<Pair>& out) {
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    order_.clear();
    bounds_.clear();
    for (uint32_t i = 0; i < n; ++i) {
        order_.push_back(i);
        bounds_.push_back(bounds(bodies[i]));
    }

    // Компаратор обязан быть ПОЛНЫМ порядком, а не просто «по возрастанию x»: при равных x
    // (а это норма — сетка тайлов, стопка ящиков) порядок равных элементов у `std::sort` не
    // определён стандартом, значит зависел бы от реализации библиотеки, то есть от ОС. Ключ
    // тела уникален по контракту, поэтому добивка ключом делает порядок единственным.
    const auto& bnd = bounds_;
    std::sort(order_.begin(), order_.end(), [&](uint32_t l, uint32_t r) {
        if (!(bnd[l].min.x == bnd[r].min.x)) return bnd[l].min.x < bnd[r].min.x;
        return bodies[l].key < bodies[r].key;
    });

    out.clear();
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t bi = order_[i];
        const fix32 sweep_end = bounds_[bi].max.x;
        for (uint32_t j = i + 1; j < n; ++j) {
            const uint32_t bj = order_[j];
            // Список отсортирован по левому краю, поэтому первое же тело, начинающееся правее
            // правого края текущего, закрывает и всех, кто за ним, — в этом весь выигрыш SAP.
            if (sweep_end < bounds_[bj].min.x) break;
            if (both_immovable(bodies[bi], bodies[bj])) continue;
            if (!overlaps(bounds_[bi], bounds_[bj])) continue;

            // Нормализация по ключу, а не по индексу: индекс — порядок создания, и при
            // перетасованном создании та же пара пришла бы в решатель зеркальной, а импульсы
            // накапливаются последовательно, значит зеркальная пара — другой результат.
            const bool a_first = bodies[bi].key < bodies[bj].key;
            const uint32_t a = a_first ? bi : bj;
            const uint32_t b = a_first ? bj : bi;
            out.push_back({a, b, bodies[a].key, bodies[b].key});
        }
    }

    std::sort(out.begin(), out.end(), [](const Pair& l, const Pair& r) {
        if (l.key_a != r.key_a) return l.key_a < r.key_a;
        return l.key_b < r.key_b;
    });
}

} // namespace framework::physics
