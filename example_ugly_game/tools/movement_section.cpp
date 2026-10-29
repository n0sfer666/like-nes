#include "movement_section.hpp"

#include "bakers.hpp"
#include "profile_bake.hpp"

namespace game {

bool movement_section(const std::string& src, std::vector<asset::AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::character::ProfileBakeError err;
    if (!framework::character::bake_profiles_file(src, table, err)) {
        asset::bakers::report("movement", src, err.line, err.message);
        return false;
    }
    asset::bakers::push_table("movement", std::move(table), out);
    return true;
}

} // namespace game
