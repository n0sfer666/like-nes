#pragma once
#include <webgpu/webgpu.h>

#include "batch.hpp"
#include "fx.hpp"
#include "scene_fx.hpp"
#include "world.hpp"

namespace game {

// `sfx` не привязан — сцена рисуется базовым пайплайном, как рисовалась. Так ходит offscreen-путь
// `--demo`: на его кадре стоит голден рендер-гейтов #2, и надеть на него эффекты значило бы
// перепечь эталон ради косметики (гейт 8 спеки #18 — регресс).
void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas, const SceneFx& sfx);
void push_hud(SpriteBatch& batch, flecs::world& world, const Atlas& atlas, const GameState& gs);
void push_screen(SpriteBatch& batch, const Atlas& atlas, const GameState& gs);
void push_toast(SpriteBatch& batch, const Atlas& atlas, const char* name, uint32_t left);
// Частицы подаются ОТСЮДА, а не из `Fx`: сам он про WebGPU не знает намеренно (см. `Fx::draw`), и
// подача кадра собрана в одном файле, а не в двух.
void push_fx(SpriteBatch& batch, Fx& fx, const Atlas& atlas);

} // namespace game
