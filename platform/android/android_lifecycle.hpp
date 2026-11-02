#pragma once
#include <cstdint>
namespace mobile { class Game; }
// Window/GPU lifecycle remains in native_main; this owns process pause and persistence events.
void android_game_lifecycle(mobile::Game* game, int32_t command);
