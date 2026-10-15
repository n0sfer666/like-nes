#pragma once
#include <android/asset_manager.h>

#include <string>

namespace game {

// Ассеты едут в APK сжатым архивом, а читатели движка открывают их как ФАЙЛЫ (mmap через шов
// платформы): указателя внутрь APK им не передать. Поэтому при старте все файлы корня assets/
// распаковываются в песочницу приложения, и дальше игра не знает, что запущена на Android.
// Оболочка не знает, какие из них игре обязательны: каждый недоехавший файл назван в logcat, а
// без нужного бандла откажет init игры. false — хоть один файл не доехал.
bool unpack_apk_assets(AAssetManager* am, const std::string& dir);

} // namespace game
