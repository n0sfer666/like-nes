#pragma once
#include "../core/fixed.hpp"
#include "platform_export.h"
#include <cstdint>

// Host-owned состояние (POD, fixed-point). Живёт в host, НЕ в dylib.
struct GameState {
    fix32 x;
    int32_t ticks;
};

// Stateless gameplay-граница: C-ABI (extern "C") для стабильного ABI между host и dylib.
// Функция оперирует переданным host-состоянием, ничего не владеет.
extern "C" {
    PLATFORM_EXPORT void game_tick(GameState* s);
}
