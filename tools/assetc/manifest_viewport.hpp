#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "viewport_settings.hpp"

namespace asset::manifest {
struct Error;
struct Bake;
struct ViewportLines { uint32_t zone = 0, limit = 0; };
bool viewport_key(const std::string& key);
bool read_viewport(const std::vector<std::string>& fields, uint32_t line,
                   framework::graphics::ViewportSettings& out, ViewportLines& seen, Error& error);
bool bake_viewport(Bake& bake);
} // namespace asset::manifest
