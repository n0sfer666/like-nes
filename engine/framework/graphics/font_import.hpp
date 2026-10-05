#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "font_bake.hpp"

namespace framework::graphics {

constexpr uint16_t FONT_CELLS_PER_ROW = 32;

struct FontAtlas {
    FontSrc font;
    std::vector<uint8_t> rgba;
};

bool import_bitmask_font(const std::string& name, const std::string& file, std::span<const std::byte> bytes,
                         FontAtlas& out, std::string& error);

} // namespace framework::graphics
