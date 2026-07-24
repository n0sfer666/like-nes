#pragma once
#include <string>

namespace game {

// Путь к ассету по имени: env LIKENES_ASSETS → рядом с exe → .app/Resources → dev-fallback.
// Пусто => не найден. Only exe-relative (+env) → детерм. вне зависимости от cwd.
std::string resolve_asset(const char* name);

// game.bundle (обёртка над resolve_asset).
std::string resolve_bundle_path();

} // namespace game
