#include "json.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "json_scan.hpp"

namespace framework::json {

JsonError error_at(std::span<const std::byte> in, uint32_t at, std::string message) {
    JsonError e{1, 1, std::move(message)};
    const std::size_t end = std::min<std::size_t>(at, in.size());
    for (std::size_t i = 0; i < end; ++i) {
        const uint8_t b = std::to_integer<uint8_t>(in[i]);
        if (b == '\n') {
            ++e.line;
            e.column = 1;
        } else if ((b & 0xC0) != 0x80) {
            ++e.column;
        }
    }
    return e;
}

namespace detail {

std::string printable(std::string_view key) {
    static constexpr char HEX[] = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < key.size() && i < 64; ++i) {
        const auto b = static_cast<unsigned char>(key[i]);
        if (b >= 0x20 && b < 0x7f && b != '"' && b != '\\') {
            out += static_cast<char>(b);
            continue;
        }
        out += "\\x";
        out += HEX[b >> 4];
        out += HEX[b & 15];
    }
    if (key.size() > 64) out += "...";
    return out;
}

} // namespace detail

std::string format_error(std::string_view file, const JsonError& err) {
    return std::string(file) + ":" + std::to_string(err.line) + ":" + std::to_string(err.column) +
           ": " + err.message;
}

} // namespace framework::json
