#include "utf8_decode.hpp"

namespace framework::core {
namespace {

int byte_at(std::string_view s, std::size_t i) {
    return i < s.size() ? static_cast<unsigned char>(s[i]) : -1;
}

std::size_t tail_of(int b0) {
    if (b0 >= 0xC2 && b0 <= 0xDF) return 1;
    if (b0 >= 0xE0 && b0 <= 0xEF) return 2;
    if (b0 >= 0xF0 && b0 <= 0xF4) return 3;
    return 0;
}

} // namespace

bool utf8_next(std::string_view s, std::size_t& at, uint32_t& cp) {
    if (at >= s.size()) return false;
    const int b0 = byte_at(s, at);
    if (b0 < 0x80) {
        cp = static_cast<uint32_t>(b0);
        ++at;
        return true;
    }
    cp = UTF8_INVALID;
    const std::size_t n = tail_of(b0);
    if (n == 0) {
        ++at;
        return true;
    }
    const int lo = b0 == 0xE0 ? 0xA0 : (b0 == 0xF0 ? 0x90 : 0x80);
    const int hi = b0 == 0xED ? 0x9F : (b0 == 0xF4 ? 0x8F : 0xBF);
    uint32_t value = static_cast<uint32_t>(b0) & (0x7Fu >> (n + 1));
    for (std::size_t k = 1; k <= n; ++k) {
        const int b = byte_at(s, at + k);
        if (b < (k == 1 ? lo : 0x80) || b > (k == 1 ? hi : 0xBF)) {
            ++at;
            return true;
        }
        value = (value << 6) | (static_cast<uint32_t>(b) & 0x3Fu);
    }
    cp = value;
    at += n + 1;
    return true;
}

bool utf8_valid(std::string_view s) {
    std::size_t at = 0;
    uint32_t cp = 0;
    while (utf8_next(s, at, cp))
        if (cp == UTF8_INVALID) return false;
    return true;
}

} // namespace framework::core
