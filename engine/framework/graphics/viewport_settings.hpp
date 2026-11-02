#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "viewport_fit.hpp"

namespace framework::graphics {

inline constexpr uint32_t VIEWPORT_SIDE_MAX = 32767;
struct ViewportSettings {
    PixelSize zone = VIEW_ZONE;
    PixelSize limit = VIEW_LIMIT;
};

bool viewport_settings_valid(const ViewportSettings& settings);
bool viewport_settings_default(const ViewportSettings& settings);
bool read_viewport_settings(const void* data, std::size_t size, ViewportSettings& out);
bool bake_viewport_settings(const ViewportSettings& settings, std::vector<uint8_t>& out, std::string& error);

} // namespace framework::graphics
