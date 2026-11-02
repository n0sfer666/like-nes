#include "android_lifecycle.hpp"

#include <android_native_app_glue.h>

#include "mobile_app.hpp"

void android_game_lifecycle(mobile::Game* game, int32_t command) {
    if (!game) return;
    if (command == APP_CMD_LOST_FOCUS) game->cancel();
    else if (command == APP_CMD_PAUSE) { game->cancel(); game->suspend(); }
    else if (command == APP_CMD_RESUME) game->resume();
    // native_app_glue acknowledges SAVE_STATE after the handler returns; PAUSE has no such wait.
    else if (command == APP_CMD_SAVE_STATE || command == APP_CMD_STOP) game->save();
}
