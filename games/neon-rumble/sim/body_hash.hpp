#pragma once
#include <cstdint>

#include "body_pool.hpp"

namespace framework::brawl {

uint64_t state_hash(const BodyPool& pool);

} // namespace framework::brawl
