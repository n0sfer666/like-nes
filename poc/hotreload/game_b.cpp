#include "game_api.h"

// Версия B геймплея (имитирует «разработчик отредактировал и пересобрал»): x += 10 за тик.
extern "C" void game_tick(GameState* s) {
    s->x = s->x + fix32::from_int(10);
    s->ticks++;
}
