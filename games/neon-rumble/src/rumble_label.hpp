#pragma once
#include <array>
#include <cstdint>

#include "rumble_brawl.hpp"

namespace rumble {

// В режиме волны тел одного вида несколько, и метка несёт seq; вне волны строки прежние.
struct Label { std::array<char, 32> text{}; };

Label label(const Brawl& brawl, framework::brawl::EntId id, uint8_t kind);
Label label(const Brawl& brawl, const framework::brawl::Body& b);

} // namespace rumble
