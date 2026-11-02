#pragma once
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "font_bake.hpp"
#include "strings_format.hpp"

namespace framework::ui {
struct StringSrc { std::string key, en, ru; uint32_t line = 0; };
struct StringsStats { uint32_t keys = 0, fallbacks = 0; };

// Literal UTF-8 TSV with exactly key/en/ru columns; LF/CRLF, no quoting or escaping.
bool parse_strings_tsv(std::string_view text, const std::string& file, std::vector<StringSrc>& out, std::string& error);
// Empty ru falls back to en; strict refuses fallback. Every text code point must exist in font.
// Failure keeps out unchanged and clears stats; key IDs use the shared asset FNV primitive.
bool bake_strings(std::span<const StringSrc> rows, const graphics::FontSrc& font, bool strict,
                  std::vector<uint8_t>& out, StringsStats& stats, std::string& error);
}
