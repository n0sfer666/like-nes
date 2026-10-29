#include "belt_ride.hpp"

#include "depth_move.hpp"

namespace framework::depth {

void ride_belt(DepthBody& d, fix32 belt, FloorView f) {
    if (belt == fix32{} || !grounded(d)) return;
    d.x = move_x(f, d.x, d.z, d.x + belt);
}

} // namespace framework::depth
