#include "bakers.hpp"

namespace asset::bakers {

bool fighter(const char* name, const std::string& text, std::span<const framework::graphics::ClipSrc> clips,
             std::vector<AssetInput>& out, framework::brawl::FighterBakeError& error) {
    std::vector<uint8_t> table;
    if (!framework::brawl::bake_fighter(name, text, clips, table, error)) return false;
    push_table(name, std::move(table), out);
    return true;
}

} // namespace asset::bakers
