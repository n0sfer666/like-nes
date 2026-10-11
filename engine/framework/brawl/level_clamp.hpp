#pragma once
#include "scene_objects.hpp"
#include "stage_camera.hpp"

namespace framework::brawl {

// Кламп центра вида: по запертой арене, затем по `bounds` уровня. Его же судит возврат после
// автоскролла: цель следования за клампом недостижима, и возврат к ней не кончился бы никогда.
Vec2 clamp_level(Vec2 c, int16_t arena, const scene::LevelObjects& level, Vec2 half);

} // namespace framework::brawl
