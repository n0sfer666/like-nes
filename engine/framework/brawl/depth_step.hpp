#pragma once
#include "brawl_body.hpp"
#include "brawl_input.hpp"
#include "depth_floor.hpp"
#include "depth_profile.hpp"

namespace framework::brawl {

bool grounded(const DepthBody& d);
void step_body(Body& b, const BrawlInput& in, const DepthProfile& p, const DepthFloor& f);
void coast_body(Body& b, const DepthProfile& p, const DepthFloor& f);

} // namespace framework::brawl
