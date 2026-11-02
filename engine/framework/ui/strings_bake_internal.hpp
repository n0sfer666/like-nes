#pragma once
// Private indexed stage lets tests supply two different names with the same ID, without changing FNV.
#include "strings_bake.hpp"
namespace framework::ui::detail {
struct IndexedString { uint64_t id; const StringSrc* source; };
bool bake_indexed_strings(std::span<const IndexedString> rows, const graphics::FontSrc& font, bool strict,
                          std::vector<uint8_t>& out, StringsStats& stats, std::string& error);
}
