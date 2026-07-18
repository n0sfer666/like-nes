#include "game_api.h"

// Версия с багом: разыменование null внутри gameplay — проверка крэш-изоляции.
extern "C" void game_tick(GameState* s) {
    s->ticks++;                 // успел войти
    volatile int* p = nullptr;
    *p = 42;                    // SIGSEGV в gameplay-коде
}
