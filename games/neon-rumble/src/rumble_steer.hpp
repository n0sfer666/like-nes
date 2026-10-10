#pragma once
#include <cstdint>

#include "ai_target.hpp"

namespace rumble {

// Знак шага по оси к `to`; ноль, когда до неё не больше `arrive` пикселей.
int32_t toward(fix32 from, fix32 to, int32_t arrive);

} // namespace rumble
