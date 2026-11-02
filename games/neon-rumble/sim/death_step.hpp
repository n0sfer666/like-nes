#pragma once
#include "body_pool.hpp"
#include "death_facts.hpp"
#include "living.hpp"

namespace framework::brawl {

// Смерть — переход `hp > 0 → ≤ 0` внутри тика. `hp` после шага прошлого «до» уже не помнит, поэтому
// живые снимаются в начале тика (`note_living`, до шага тел), а факты — в девятой системе
// (`step_deaths`): тело из `living`, у которого теперь `hp <= 0`. Снятое игрой посреди тика — не
// смерть; появившееся в тике с `hp <= 0` — тоже, оно не было живым. Тело, лежащее с `hp 0` дальше,
// повторно не умирает.
void note_living(const BodyPool& pool, Living& out);
void step_deaths(const BodyPool& pool, const Living& living, DeathFacts& out);

} // namespace framework::brawl
