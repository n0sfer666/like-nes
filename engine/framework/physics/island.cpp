#include "island.hpp"

namespace framework::physics {
namespace {

bool joins(const Body& b) { return b.type != BodyType::Static; }

} // namespace

void Islands::reserve(uint32_t capacity) {
    parent_.reserve(capacity);
    root_.reserve(capacity);
}

uint32_t Islands::find(uint32_t i) {
    // Сжатие путей ИТЕРАЦИЕЙ, а не рекурсией: длина цепочки здесь — это высота башни, которую
    // построила игра, то есть входные данные. Рекурсия сделала бы глубину стека функцией уровня.
    uint32_t r = i;
    while (parent_[r] != r) r = parent_[r];
    while (parent_[i] != r) {
        const uint32_t next = parent_[i];
        parent_[i] = r;
        i = next;
    }
    return r;
}

void Islands::build(const std::vector<Body>& bodies, const std::vector<Manifold>& contacts,
                    const std::vector<Manifold>& resting) {
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    parent_.clear();
    for (uint32_t i = 0; i < n; ++i) parent_.push_back(i);

    const auto unite = [&](const std::vector<Manifold>& list) {
        for (const Manifold& m : list) {
            if (!joins(bodies[m.a]) || !joins(bodies[m.b])) continue;
            const uint32_t ra = find(m.a);
            const uint32_t rb = find(m.b);
            if (ra == rb) continue;
            // Корнем становится МЕНЬШИЙ индекс — отсюда и обещание `root()` про минимум. Объединение
            // по рангу дало бы дерево пониже, но представителя, зависящего от порядка рёбер: та же
            // сцена, собранная в другом порядке, получила бы другие ярлыки островов, и всё, что на
            // ярлык опирается, пришлось бы объявлять «деталью реализации».
            if (ra < rb) {
                parent_[rb] = ra;
            } else {
                parent_[ra] = rb;
            }
        }
    };
    unite(contacts);
    unite(resting);

    root_.clear();
    for (uint32_t i = 0; i < n; ++i) root_.push_back(find(i));
}

} // namespace framework::physics
