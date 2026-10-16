#pragma once
#include <string>

namespace mobile {

// Куда android_main распаковал ассеты APK: asset_path() отвечает путями внутри этого каталога.
void set_asset_dir(std::string dir);

} // namespace mobile
