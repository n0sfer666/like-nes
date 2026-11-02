#pragma once
#include <string>

namespace game {

// Путь к ассету по имени: env LIKENES_ASSETS → рядом с exe → .app/Resources → dev-fallback.
// Пусто => не найден. Only exe-relative (+env) → детерм. вне зависимости от cwd.
std::string resolve_asset(const char* name);

// game.bundle (обёртка над resolve_asset).
std::string resolve_bundle_path();

// Live/mobile startup only; demo does not load/move real user profiles. Failure is nonfatal.
bool init_save_paths();
// save::dir + canonical filename, empty on uninitialized/unavailable root; no exe/cwd fallback.
std::string resolve_save_path(const char* name);
std::string achievement_save_path(); // Retry provider: prepare directory/legacy before late ACH attach.

} // namespace game
