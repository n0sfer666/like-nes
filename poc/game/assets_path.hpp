#pragma once
#include <string>

namespace game {

// Путь к ассету по имени: env LIKENES_ASSETS → рядом с exe → .app/Resources → dev-fallback.
// Пусто => не найден. Only exe-relative (+env) → детерм. вне зависимости от cwd.
std::string resolve_asset(const char* name);

// game.bundle (обёртка над resolve_asset).
std::string resolve_bundle_path();

// Записываемый путь для сейва: env LIKENES_SAVE_DIR → пользовательский каталог данных ОС
// (%APPDATA% / ~/Library/Application Support / $XDG_DATA_HOME) → рядом с exe. Каталог создаётся.
std::string resolve_save_path(const char* name);

} // namespace game
