#pragma once
#include "brawl_body.hpp"
#include "brawl_floor.hpp"
#include "brawl_input.hpp"
#include "depth_move.hpp"
#include "depth_profile.hpp"

namespace framework::brawl {

using depth::DepthProfile;
using depth::grounded;

void step_body(Body& b, const BrawlInput& in, const DepthProfile& p, const DepthFloor& f);
void coast_body(Body& b, const DepthProfile& p, const DepthFloor& f);

} // namespace framework::brawl
