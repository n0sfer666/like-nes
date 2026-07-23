#pragma once
#include <string>

namespace game {

// Путь к game.bundle: env LIKENES_ASSETS → рядом с exe → .app/Resources → dev-fallback.
// Пусто => не найден (вызывающий делает fallback на процедурный atlas).
std::string resolve_bundle_path();

} // namespace game
