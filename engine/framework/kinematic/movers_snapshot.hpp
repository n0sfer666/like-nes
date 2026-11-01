#pragma once
#include <vector>

#include "snapshot.hpp"
#include "movers.hpp"

namespace framework::kinematic {

// Снапшот физмира платформ — мир целиком и следования. Одно без другого откатывает платформу в
// чужое место: тело без следования поедет к старой цели, следование без тела — от нового места.
struct MoversSnapshot {
    physics::WorldSnapshot world;
    std::vector<PathFollow> follow;
};

void save_movers(const Movers& m, MoversSnapshot& out);
void restore_movers(const MoversSnapshot& s, Movers& m);

} // namespace framework::kinematic
