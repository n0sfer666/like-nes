#pragma once
#include <android/asset_manager.h>

#include <string>

namespace game {

// Бандлы игры едут в APK сжатым архивом, а читатели движка открывают их как ФАЙЛЫ (mmap через шов
// платформы): указателя внутрь APK им не передать. Поэтому при старте они распаковываются в
// песочницу приложения, и дальше игра не знает, что запущена на Android.
// true — доехал game.bundle: без него игры нет, а без материалов или звука она деградирует, как
// на десктопе. Каждый недоехавший бандл назван в logcat.
bool unpack_apk_assets(AAssetManager* am, const std::string& dir);

} // namespace game
