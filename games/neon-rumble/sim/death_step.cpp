#include "death_step.hpp"

namespace framework::brawl {

void note_living(const BodyPool& pool, Living& out) {
    out = Living{};
    for (uint32_t i = 0; i < pool.count; ++i)
        if (pool.bodies[i].hp > 0) out.at[out.count++] = pool.bodies[i].id;
}

void step_deaths(const BodyPool& pool, const Living& living, DeathFacts& out) {
    out = DeathFacts{};
    for (uint32_t i = 0; i < living.count; ++i) {
        const Body* b = pool.find(living.at[i]);
        if (b != nullptr && b->hp <= 0) out.at[out.count++] = Death{b->id};
    }
}

} // namespace framework::brawl
