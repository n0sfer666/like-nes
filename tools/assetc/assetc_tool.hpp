#pragma once
#include <string>

namespace asset {

// Имя инструмента для usage и советов отказа: ядро общее у `assetc`, `ugly_assetc` и
// `rumble_assetc`, и совет «перепеки `assetc --game`» из чужого бинаря отправлял бы не туда.
std::string tool_name(const char* argv0);

} // namespace asset
