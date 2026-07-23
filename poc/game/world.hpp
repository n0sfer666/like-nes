#pragma once
#include "fixed.hpp"
#include "action_map.hpp"
#include <flecs.h>
#include <cstdint>

namespace game {

constexpr int VIEW_W = 960;
constexpr int VIEW_H = 540;
constexpr int HALF_W = VIEW_W / 2;
constexpr int HALF_H = VIEW_H / 2;

enum Action { A_Fire = 0 };
enum Axis { AX_MoveX = 0, AX_MoveY = 1 };

struct Transform { fix32 x, y; };
struct Velocity { fix32 x, y; };
struct Ship {};
struct Star { fix32 speed; fix32 size; uint8_t shade; };

input::ActionMap make_map();

} // namespace game
