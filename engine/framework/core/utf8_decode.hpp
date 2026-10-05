#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace framework::core {

constexpr uint32_t UTF8_INVALID = 0xFFFFFFFFu;

bool utf8_next(std::string_view s, std::size_t& at, uint32_t& cp);
bool utf8_valid(std::string_view s);

} // namespace framework::core
