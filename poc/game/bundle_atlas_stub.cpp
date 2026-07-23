#include "art.hpp"

// WIN32-stub: basis-транскодер (asset_gpu) — POSIX-only (как asset_render). На Windows
// baked-путь недоступен → игра рендерит процедурный atlas (паритет поведения, не байтов).

namespace game {

Atlas load_baked_atlas(const char*) {
    Atlas a;
    set_regions(a);
    return a; // bc7 пуст → RGBA-путь
}

Atlas load_game_atlas(bool) { return build_atlas(); }

} // namespace game
