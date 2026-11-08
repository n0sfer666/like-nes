#pragma once
#include <string>

struct ANativeActivity;

namespace mobile {

// Куда android_main распаковал ассеты APK: asset_path() отвечает путями внутри этого каталога.
void set_asset_dir(std::string dir);
void set_demo(bool enabled);
void set_args(std::string flags, ANativeActivity* activity = nullptr);
ANativeActivity* native_activity();

} // namespace mobile
