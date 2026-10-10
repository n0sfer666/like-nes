#pragma once
#include <cstdint>

namespace framework::ai {

// ГСЧ симуляции — один на мир и целиком в снапшоте. ИИ не зовёт ничего вне симуляции: переигранный
// тик, взявший число из другого источника, выбрал бы другое, и откат разошёлся бы с прямым прогоном.
struct WorldRng {
    uint64_t state = 0;
};

uint32_t next_u32(WorldRng& rng);
uint32_t below(WorldRng& rng, uint32_t n);

} // namespace framework::ai
