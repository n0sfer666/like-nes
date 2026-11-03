#include "ui_edit.hpp"

#include <algorithm>
#include <cstring>

#include "ui_context.hpp"
#include "utf8_decode.hpp"

namespace framework::ui::edit {
namespace {
bool printable(uint32_t cp) {
    return cp >= 32 && cp <= 0x10ffff && !(cp >= 0x7f && cp < 0xa0) && !(cp >= 0xd800 && cp <= 0xdfff);
}
}
bool Alphabet::open(std::string_view text) {
    size = 0;
    if (text.empty() || text.size() > MAX_ALPHABET * 4) return false;
    size_t at = 0; uint32_t cp;
    while (core::utf8_next(text, at, cp)) {
        if (size == MAX_ALPHABET || !printable(cp) || contains(cp)) return false;
        codes[size++] = cp;
    }
    return true;
}
bool Alphabet::contains(uint32_t cp) const { return std::find(codes.begin(), codes.begin() + size, cp) != codes.begin() + size; }
size_t encode(uint32_t cp, std::span<char, 4> out) {
    if (!printable(cp)) return 0;
    if (cp < 0x80) { out[0] = static_cast<char>(cp); return 1; }
    if (cp < 0x800) {
        out[0] = static_cast<char>(0xc0 | (cp >> 6)); out[1] = static_cast<char>(0x80 | (cp & 63)); return 2;
    }
    if (cp < 0x10000) {
        out[0] = static_cast<char>(0xe0 | (cp >> 12)); out[1] = static_cast<char>(0x80 | ((cp >> 6) & 63));
        out[2] = static_cast<char>(0x80 | (cp & 63)); return 3;
    }
    out[0] = static_cast<char>(0xf0 | (cp >> 18)); out[1] = static_cast<char>(0x80 | ((cp >> 12) & 63));
    out[2] = static_cast<char>(0x80 | ((cp >> 6) & 63)); out[3] = static_cast<char>(0x80 | (cp & 63)); return 4;
}
bool Buffer::open(std::span<char> source) {
    bytes = source.first(std::min(source.size(), UI_FIELD_BYTES)); length = 0; count = 0;
    if (!bytes.data() || bytes.empty()) return false;
    const auto* end = static_cast<const char*>(std::memchr(bytes.data(), 0, bytes.size()));
    if (!end) return false;
    length = static_cast<size_t>(end - bytes.data());
    size_t at = 0; uint32_t cp;
    while (core::utf8_next({bytes.data(), length}, at, cp)) { if (!printable(cp)) return false; ++count; }
    return true;
}
bool Buffer::append(uint32_t cp, uint32_t max_len) {
    std::array<char, 4> encoded;
    const auto n = encode(cp, encoded);
    if (!n || count >= max_len || n >= bytes.size() - length) return false;
    std::copy_n(encoded.begin(), n, bytes.begin() + length); length += n; ++count; bytes[length] = 0;
    return true;
}
bool Buffer::erase() {
    if (!length) return false;
    do { --length; } while (length && (static_cast<unsigned char>(bytes[length]) & 0xc0) == 0x80);
    bytes[length] = 0; --count; return true;
}
}
