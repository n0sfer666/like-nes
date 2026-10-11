#pragma once
#include <cstdint>

#include "fixmath.hpp"
#include "stage_camera.hpp"

namespace framework::brawl {

constexpr int32_t NO_WAVE = -1;

// Прогресс арен в хеше и снапшоте. Арены проходятся по возрастанию `id`: `cleared` — последняя
// зачищенная, запереть можно только следующую за ней. `wave` — значение свойства `wave` идущей
// волны запертой арены (номера с пропусками допустимы), `lull` — тиков до старта следующей.
// `seen` — `center.x` камеры прошлого шага: замок судит пересечение, а не точное попадание, и игра
// при открытии уровня ставит его равным центру камеры, иначе первый шаг «пересечёт» путь от нуля.
struct StageWaves {
    int16_t cleared = NO_ARENA;
    int32_t wave = NO_WAVE;
    uint16_t lull = 0;
    fix32 seen{};
};

// Пауза между зачисткой волны и стартом следующей — данные игры. После последней волны замок
// снимается без паузы.
struct WaveRules {
    uint16_t lull = 0;
};

} // namespace framework::brawl
