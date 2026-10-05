#include "bakers.hpp"

namespace asset::bakers {

bool fonts(std::span<const framework::graphics::FontSrc> fonts, std::vector<AssetInput>& out, std::string& error) {
    std::vector<uint8_t> table;
    if (!framework::graphics::bake_fonts(fonts, table, error)) return false;
    push_table("fonts", std::move(table), out);
    return true;
}

} // namespace asset::bakers
