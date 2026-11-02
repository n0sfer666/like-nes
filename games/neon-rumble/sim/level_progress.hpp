#pragma once
#include <cstdint>

namespace framework::brawl {

constexpr int16_t NO_CHECKPOINT = -1;

// Прогресс уровня в хеше и снапшоте: `order` последнего взятого чекпоинта. Только растёт —
// респаун у чекпоинта и раздельные жизни читают его, а не двигают назад. Зачищенные арены —
// `StageWaves::cleared`, здесь их нет.
struct LevelProgress {
    int16_t checkpoint = NO_CHECKPOINT;
};

} // namespace framework::brawl
