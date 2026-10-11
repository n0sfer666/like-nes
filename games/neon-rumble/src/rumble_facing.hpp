#pragma once
#include <cstdint>

#include "object_read.hpp"

namespace rumble {

// Свойство `facing` точки уровня: "left" — влево, всё остальное и его отсутствие — вправо.
int8_t facing_of(const framework::tilemap::ObjectMap& map, const framework::tilemap::MapObject& o);

} // namespace rumble
