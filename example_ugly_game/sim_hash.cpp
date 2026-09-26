#include "sim_hash.hpp"
#include "../engine/asset/hash.hpp"

#include <algorithm>
#include <vector>

namespace game {
namespace {

struct Entry { flecs::entity e; int32_t x, y; uint32_t seq; };

} // namespace

uint64_t sim_hash(flecs::world& world, const GameState& gs) {
    std::vector<Entry> ents;
    world.each([&](flecs::entity e, Transform& t, EntId& id) {
        ents.push_back({e, t.x.raw, t.y.raw, id.seq});
    });
    std::sort(ents.begin(), ents.end(), [](const Entry& a, const Entry& b) { return a.seq < b.seq; });

    // Смешивание ЦЕЛЫМ СЛОВОМ, а не побайтно: свести его к байтовой форме значило бы получить
    // ДРУГОЕ число и перештамповать голден игры ради косметики (находка 5 аудита #21).
    uint64_t h = asset::FNV_OFFSET;
    auto mix = [&](uint64_t v) { h = asset::fnv1a_word(h, v); };
    mix(gs.tick); mix(gs.seq); mix(gs.score); mix((uint32_t)gs.lives); mix(gs.rng);
    mix(gs.fire_cd); mix(gs.spawn_cd); mix(gs.phase); mix(gs.phase_t); mix(gs.kills);
    for (const Entry& it : ents) {
        uint32_t kind = it.e.has<Enemy>() ? 2u : it.e.has<Bullet>() ? 1u
                        : it.e.has<Hostile>() ? 3u : it.e.has<Boss>() ? 4u : 0u;
        mix(it.seq); mix(kind); mix((uint32_t)it.x); mix((uint32_t)it.y);
        if (kind == 2u) mix((uint32_t)it.e.get<Enemy>().hp);
        if (kind == 4u) mix((uint32_t)it.e.get<Boss>().hp);
    }
    return h;
}

} // namespace game
