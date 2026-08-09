#include "state_hash.hpp"

#include <algorithm>

#include "hash_mix.hpp"

namespace framework::physics {

uint64_t state_hash(const std::vector<Body>& bodies, std::vector<uint32_t>& scratch) {
    scratch.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies.size()); ++i) scratch.push_back(i);
    std::sort(scratch.begin(), scratch.end(),
              [&](uint32_t l, uint32_t r) { return bodies[l].key < bodies[r].key; });

    uint64_t h = FNV_OFFSET;
    for (uint32_t i : scratch) {
        const Body& b = bodies[i];
        // Ключ входит в хеш наравне с состоянием: без него две сцены, где тела обменялись
        // позициями, дали бы один хеш, и гейт 2 перестал бы отличать «тот же результат» от
        // «те же значения в другом порядке».
        mix(h, b.key);
        mix(h, static_cast<uint32_t>(b.position.x.raw));
        mix(h, static_cast<uint32_t>(b.position.y.raw));
        mix(h, static_cast<uint32_t>(b.velocity.x.raw));
        mix(h, static_cast<uint32_t>(b.velocity.y.raw));
        // Угол и угловая скорость — такое же состояние, как позиция и скорость. Не включить их
        // значило бы получить гейт, который не отличает лежащий ящик от стоящего на ребре и молча
        // пропускает любое расхождение вращения между платформами. `rot` при этом НЕ хешируется: он
        // функция угла, и хешировать вычислимое — это ловить не расхождение состояния, а
        // расхождение таблицы синуса, для которого есть собственный гейт.
        mix(h, static_cast<uint32_t>(b.angle.raw));
        mix(h, static_cast<uint32_t>(b.angular_velocity.raw));
    }
    return h;
}

} // namespace framework::physics
