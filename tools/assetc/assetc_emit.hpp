#pragma once
#include <string>
#include <vector>

#include "bundle_writer.hpp"

namespace asset {

// Бандл на диск + строки `[assetc] <что> …` и `bundle_hash`, которые читают гейты. Код выхода
// процесса: 0 — записан, 1 — нет.
int emit(const char* what, const std::string& path, std::vector<AssetInput> assets);

} // namespace asset
