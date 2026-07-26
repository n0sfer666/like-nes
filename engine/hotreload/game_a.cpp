#include "game_api.h"

// Версия A геймплея: x += 1 за тик.
extern "C" void game_tick(GameState* s) {
    s->x = s->x + fix32::from_int(1);
    s->ticks++;
}
