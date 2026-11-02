#include "strings_bake_internal.hpp"

#include <algorithm>
#include <cstdio>

#include "section_bake.hpp"
#include "strings.hpp"
#include "utf8_decode.hpp"

namespace framework::ui {
namespace {
bool covered(std::string_view text, const std::vector<uint32_t>& glyphs, const std::string& who, std::string& error) {
    if (text.empty()) { error = who + " is empty"; return false; }
    size_t at = 0; uint32_t cp;
    while (core::utf8_next(text, at, cp)) {
        if (cp == 0 || cp == core::UTF8_INVALID || !std::binary_search(glyphs.begin(), glyphs.end(), cp)) {
            char hex[24]; std::snprintf(hex, sizeof hex, "U+%04X", cp);
            error = who + " has an invalid or uncovered code point " + hex;
            return false;
        }
    }
    return true;
}
}
bool detail::bake_indexed_strings(std::span<const IndexedString> indexed, const graphics::FontSrc& font, bool strict,
                                 std::vector<uint8_t>& out, StringsStats& stats, std::string& error) {
    stats = {}; error.clear();
    if (indexed.empty() || indexed.size() > MAX_STRINGS) { error = "expected 1 to 4096 string keys"; return false; }
    if (!graphics::check_fonts({&font, 1}, error) || !string_key_ok(font.name)) {
        if (error.empty()) error = "invalid strings font name";
        return false;
    }
    std::vector<IndexedString> sorted(indexed.begin(), indexed.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::vector<uint32_t> glyphs;
    for (const auto& glyph : font.glyphs) glyphs.push_back(glyph.codepoint);
    std::sort(glyphs.begin(), glyphs.end());
    std::vector<StringRow> rows;
    core::SectionBuilder builder(sorted.size() * sizeof(StringRow));
    StringsMeta meta{builder.text(font.name), 0, 0, 0};
    auto text = [&](const std::string& value, uint32_t& offset) {
        offset = builder.text(value);
        if (builder.encoded_size() + sizeof meta <= MAX_STRINGS_BYTES) return true;
        error = "strings exceed 16 MiB";
        return false;
    };
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (!sorted[i].source) { error = "null string source"; return false; }
        const auto& src = *sorted[i].source;
        const std::string who = (src.line ? "line " + std::to_string(src.line) + ": " : "") + "key '" + src.key + "'";
        if (!string_key_ok(src.key)) { error = who + " is invalid"; return false; }
        if (i && sorted[i - 1].id == sorted[i].id) { error = who + " has a duplicate key or FNV collision"; return false; }
        const bool fallback = src.ru.empty();
        if (strict && fallback) { error = who + " falls back to en under --strict"; return false; }
        if (src.en.size() > MAX_STRINGS_BYTES || src.ru.size() > MAX_STRINGS_BYTES) { error = who + " exceeds 16 MiB"; return false; }
        if (!covered(src.en, glyphs, who + " en", error) || (!fallback && !covered(src.ru, glyphs, who + " ru", error))) return false;
        uint32_t en, ru, key;
        if (!text(src.en, en) || !text(fallback ? src.en : src.ru, ru) || !text(src.key, key)) return false;
        rows.push_back({sorted[i].id, key, en, ru, fallback ? 1u : 0u,
                       static_cast<uint32_t>(src.en.size()), static_cast<uint32_t>(fallback ? src.en.size() : src.ru.size())});
        if (fallback) ++meta.fallbacks;
    }
    builder.block(&meta, sizeof meta, alignof(StringsMeta));
    std::vector<uint8_t> bytes;
    if (!builder.finish(STRINGS_MAGIC, STRINGS_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), bytes, error)) return false;
    if (bytes.size() > MAX_STRINGS_BYTES) { error = "strings exceed 16 MiB"; return false; }
    out.swap(bytes); stats = {static_cast<uint32_t>(rows.size()), meta.fallbacks};
    return true;
}
bool bake_strings(std::span<const StringSrc> rows, const graphics::FontSrc& font, bool strict,
                  std::vector<uint8_t>& out, StringsStats& stats, std::string& error) {
    if (rows.empty() || rows.size() > MAX_STRINGS) { stats = {}; error = "expected 1 to 4096 string keys"; return false; }
    std::vector<detail::IndexedString> indexed;
    indexed.reserve(rows.size());
    for (const auto& row : rows) indexed.push_back({string_id(row.key), &row});
    return detail::bake_indexed_strings(indexed, font, strict, out, stats, error);
}
}
