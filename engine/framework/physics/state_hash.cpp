#include "state_hash.hpp"

#include <algorithm>

namespace framework::physics {
namespace {

constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;

void mix(uint64_t& h, uint32_t v) {
    // Байтами, а не целым словом: смешивание четырёх байт по одному не зависит от порядка байт в
    // машине, а `h ^= v` — зависело бы. Хеш сверяется между x86 и ARM, и на ARM он обязан совпасть
    // не потому, что обе платформы little-endian сегодня, а потому, что вопрос здесь не задаётся.
    for (int i = 0; i < 4; ++i) {
        h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xffu);
        h *= FNV_PRIME;
    }
}

} // namespace

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
    }
    return h;
}

} // namespace framework::physics
