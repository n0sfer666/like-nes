#include "bakers.hpp"

namespace asset::bakers {

bool clips(std::span<const framework::graphics::ClipSrc> clips, std::vector<AssetInput>& out, std::string& error) {
    std::vector<uint8_t> table;
    if (!framework::graphics::bake_clips(clips, table, error)) return false;
    push_table("clips", std::move(table), out);
    return true;
}

} // namespace asset::bakers
