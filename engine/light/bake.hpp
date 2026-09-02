#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "bake_spec.hpp"

namespace light {

bool bake_lights(const std::string& text, std::vector<uint8_t>& out, BakeError& err);
bool bake_lights_file(const std::string& path, std::vector<uint8_t>& out, BakeError& err);

} // namespace light
