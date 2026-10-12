#pragma once
#include <memory>
#include <string>

#include "bundle_view.hpp"
#include "rumble.hpp"
#include "rumble_fighter.hpp"
#include "rumble_level.hpp"
#include "scene_stack.hpp"

namespace rumble {

// Уровень, в который вошли из меню: живёт от входа до выхода в меню и при следующем входе грузится
// заново. Сцена стека — rollback: переходы подаёт код игры после `run`, а не система драки.
// Объект не перемещается — расписание держит указатель на `scene`, драка — на уровень и бойцов.
struct Play {
    Level level;
    Fighters fighters;
    Brawl brawl;
    Scene scene;

    framework::scene::Scene stack_scene() { return {&scene.schedule, &scene, true}; }
};

// Отказ — nullptr и текст в `error`: его печатает строка `menu: <ошибка>` и показывает меню.
std::unique_ptr<Play> enter_level(const asset::BundleView& bundle, const char* name, bool waves, std::string& error);

} // namespace rumble
