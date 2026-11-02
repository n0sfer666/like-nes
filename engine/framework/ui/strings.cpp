#include "strings.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "font_format.hpp"
#include "hash.hpp"
#include "utf8_decode.hpp"

namespace framework::ui {
namespace {
bool text_at(const core::SectionView& v, uint32_t offset) {
    return offset > 0 && offset < v.strings_size && v.strings[offset - 1] == '\0' && v.strings[offset] != '\0';
}
bool pool_ok(const core::SectionView& v) {
    if (v.strings[0] != '\0') return false;
    const std::string_view pool(v.strings, v.strings_size);
    size_t at = 0; uint32_t cp;
    while (core::utf8_next(pool, at, cp))
        if (cp != 0 && !graphics::font_codepoint_ok(cp)) return false;
    return true;
}
struct ValueRef { uint32_t offset, size; };
bool values_ok(const core::SectionView& v, std::span<ValueRef> values) {
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.offset < b.offset; });
    for (size_t i = 0; i < values.size(); ++i) {
        const auto& value = values[i];
        if (i && value.offset == values[i - 1].offset) {
            if (value.size != values[i - 1].size) return false;
            continue;
        }
        if (!text_at(v, value.offset) || value.size == 0 || uint64_t{value.offset} + value.size >= v.strings_size ||
            v.strings[value.offset + value.size] != '\0' || std::memchr(v.strings + value.offset, 0, value.size)) return false;
    }
    return true;
}
}
uint64_t string_id(std::string_view key) { return asset::fnv1a(key.data(), key.size()); }
bool string_key_ok(std::string_view key) {
    return !key.empty() && key.size() <= MAX_STRING_KEY && std::all_of(key.begin(), key.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}
bool Strings::open(const void* data, size_t size) {
    view_ = {}; rows_ = nullptr; meta_ = nullptr;
    core::SectionView v;
    if (size > MAX_STRINGS_BYTES || !core::open_section(data, size, STRINGS_MAGIC, STRINGS_VERSION,
        sizeof(StringRow), alignof(StringRow), v)) return false;
    if (v.header->total_size != size || v.header->count == 0 || v.header->count > MAX_STRINGS ||
        uint64_t{v.rows_end} + sizeof(StringsMeta) != v.header->strings_offset || !pool_ok(v)) return false;
    std::span<const StringsMeta> meta;
    if (!v.take(v.rows_end, 1, meta) || meta[0].reserved0 || meta[0].reserved1 || !text_at(v, meta[0].font)) return false;
    const auto rows = v.at<StringRow>(v.header->rows_offset, v.header->count);
    // Shared values are checked once, even when thousands of rows reference one large phrase.
    std::array<ValueRef, 2 * MAX_STRINGS> values;
    uint32_t fallback_count = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        if (!text_at(v, r.key) || !text_at(v, r.en) || !text_at(v, r.ru) || r.fallback > 1 ||
            (r.fallback && r.en != r.ru) || (i && rows[i - 1].id >= r.id)) return false;
        // A bound before strlen prevents adversarial rows from repeatedly scanning a large pool.
        const size_t remaining = v.strings_size - r.key;
        const void* end = std::memchr(v.strings + r.key, 0, std::min(remaining, MAX_STRING_KEY + 1));
        if (!end) return false;
        const std::string_view key(v.strings + r.key, static_cast<const char*>(end) - (v.strings + r.key));
        if (!string_key_ok(key) || string_id(key) != r.id) return false;
        fallback_count += r.fallback;
        values[2 * i] = {r.en, r.en_size}; values[2 * i + 1] = {r.ru, r.ru_size};
    }
    if (fallback_count != meta[0].fallbacks || !values_ok(v, {values.data(), 2 * rows.size()})) return false;
    view_ = v; rows_ = rows.data(); meta_ = meta.data();
    return true;
}
const char* Strings::key(uint32_t index) const { return index < count() ? view_.strings + rows_[index].key : ""; }
const StringRow* Strings::find_row(uint64_t id) const {
    if (!rows_) return nullptr;
    const auto* end = rows_ + count();
    const auto* row = std::lower_bound(rows_, end, id, [](const StringRow& r, uint64_t wanted) { return r.id < wanted; });
    return row != end && row->id == id ? row : nullptr;
}
std::string_view Strings::find(uint64_t id, Language language) const {
    if (language != Language::En && language != Language::Ru) return {};
    const auto* row = find_row(id);
    if (!row) return {};
    return language == Language::En ? std::string_view(view_.strings + row->en, row->en_size) :
                                     std::string_view(view_.strings + row->ru, row->ru_size);
}
std::string_view Strings::find(std::string_view key, Language language) const {
    if (!string_key_ok(key)) return {};
    const auto* row = find_row(string_id(key));
    return row && key == std::string_view(view_.strings + row->key) ? find(row->id, language) : std::string_view{};
}
}
