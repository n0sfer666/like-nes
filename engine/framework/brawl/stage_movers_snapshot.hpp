#pragma once
#include <vector>

#include "snapshot.hpp"
#include "stage_movers.hpp"

namespace framework::brawl {

// Снапшот физмира драки — мир целиком и следования. Одно без другого откатывает платформу в
// чужое место: тело без следования поедет к старой цели, следование без тела — от нового места.
struct MoversSnapshot {
    physics::WorldSnapshot world;
    std::vector<PathFollow> follow;
};

void save_movers(const StageMovers& m, MoversSnapshot& out);
void restore_movers(const MoversSnapshot& s, StageMovers& m);

} // namespace framework::brawl
