#include "fighter_kind.hpp"

#include "fighter_bake.hpp"

namespace rumble {
namespace {

bool bake_fighter(const char* name, const std::string& text, std::span<const framework::graphics::ClipSrc> clips,
                  std::vector<uint8_t>& table, asset::manifest::KindError& error) {
    framework::brawl::FighterBakeError err;
    if (framework::brawl::bake_fighter(name, text, clips, table, err)) return true;
    error.line = err.line > 0 ? static_cast<uint32_t>(err.line) : 0;
    error.message = err.message;
    return false;
}

const asset::manifest::Kind KINDS[] = {{"fighter", ".fighter", bake_fighter}};

} // namespace

std::span<const asset::manifest::Kind> assetc_kinds() { return KINDS; }

} // namespace rumble
