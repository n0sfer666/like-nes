#include "depth_step.hpp"

namespace framework::brawl {

namespace {

depth::DepthIntent intent_of(const BrawlInput& in) {
    if (!in.present) return {};
    return depth::DepthIntent{in.move.move_x, in.move.move_z, (in.buttons & button::JUMP) != 0};
}

} // namespace

void step_body(Body& b, const BrawlInput& in, const DepthProfile& p, const DepthFloor& f) {
    const depth::DepthIntent intent = intent_of(in);
    if (intent.mx < fix32{}) b.facing = -1;
    if (fix32{} < intent.mx) b.facing = 1;
    depth::drive(b.pos, intent, p);
    coast_body(b, p, f);
}

void coast_body(Body& b, const DepthProfile& p, const DepthFloor& f) {
    depth::fall(b.pos, p);
    slide(f, b.pos);
    ++b.age;
}

} // namespace framework::brawl
