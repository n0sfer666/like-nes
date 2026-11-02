#pragma once
#include "archetype.hpp"
#include "brawl_body.hpp"
#include "brawl_input.hpp"

namespace framework::brawl {

void track_run(Body& b, const BrawlInput& in, const Archetype& a);
DepthProfile run_profile(const Body& b, const Archetype& a);
void slide_strike(Body& b, const Archetype& a);

} // namespace framework::brawl
