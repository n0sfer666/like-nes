#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace framework::ui::edit {
inline constexpr uint32_t MAX_ALPHABET = 126;
struct Alphabet {
    std::array<uint32_t, MAX_ALPHABET> codes{};
    uint32_t size = 0;
    bool open(std::string_view text);
    bool contains(uint32_t cp) const;
};
size_t encode(uint32_t cp, std::span<char, 4> out);
struct Buffer {
    std::span<char> bytes;
    size_t length = 0;
    uint32_t count = 0;
    bool open(std::span<char> source);
    bool append(uint32_t cp, uint32_t max_len);
    bool erase();
};
}
