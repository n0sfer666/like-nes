#pragma once
#include <array>
#include <cstdint>

#include "archetype.hpp"
#include "rumble_roster.hpp"

namespace rumble {

// Нож и ящик — виды без клипа. Бокс ящика выше решения 16×16: попадание судится и по высоте, а
// самый низкий удар стоя (кадр 54 kick) кончается на 31 px над полом.
void open_clipless(std::array<framework::brawl::Archetype, KINDS>& types);

} // namespace rumble
