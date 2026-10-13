#include "stage_movers_snapshot.hpp"

namespace framework::brawl {

void save_movers(const StageMovers& m, MoversSnapshot& out) {
    out.world.capture(m.world);
    out.follow = m.follow;
}

void restore_movers(const MoversSnapshot& s, StageMovers& m) {
    s.world.apply(m.world);
    m.follow = s.follow;
}

} // namespace framework::brawl
