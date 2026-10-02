#pragma once
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "clip_bake.hpp"
#include "tiled_import.hpp"

namespace framework::graphics {

bool import_aseprite(const std::string& record, const std::string& file, std::span<const std::byte> bytes,
                     tiled::Source& src, std::vector<ClipSrc>& out, std::string& error);

bool import_sheet(const std::string& record, const std::string& file, std::string_view text, tiled::Source& src,
                  std::vector<ClipSrc>& out, std::string& error);

} // namespace framework::graphics
