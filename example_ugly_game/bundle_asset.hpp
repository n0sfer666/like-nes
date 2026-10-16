#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace game {

// Ассет бандла по имени — копией байт: таблицы читают чужую память, а бандл закрывается сразу после
// чтения. false — бандл не открылся или ассета в нём нет.
bool read_bundle_asset(const std::string& bundle_path, const char* name, std::vector<uint8_t>& out);

} // namespace game
