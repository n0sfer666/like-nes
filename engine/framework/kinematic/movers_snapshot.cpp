#include "movers_snapshot.hpp"

namespace framework::kinematic {

void save_movers(const Movers& m, MoversSnapshot& out) {
    out.world.capture(m.world);
    out.follow = m.follow;
}

void restore_movers(const MoversSnapshot& s, Movers& m) {
    s.world.apply(m.world);
    m.follow = s.follow;
}

} // namespace framework::kinematic
