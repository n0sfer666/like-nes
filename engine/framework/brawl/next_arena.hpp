#pragma once
#include <cstdint>

#include "scene_objects.hpp"

namespace framework::brawl {

// Следующая арена после зачищенной `cleared` — наименьший `id` больше неё; `nullptr`, когда все
// сняты. Одно правило для замка волн и для игры, судящей «уровень пройден» (полоса до конца
// секций): копия цикла у игры разошлась бы с замком на границе `id`. `id` вне [0, 32767] не
// запирается и в порядок не входит.
const scene::Arena* next_arena(const scene::LevelObjects& level, int16_t cleared);

} // namespace framework::brawl
