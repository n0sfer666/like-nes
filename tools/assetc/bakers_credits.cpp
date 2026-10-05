#include "bakers.hpp"

namespace asset::bakers {

bool credits(const char* name, std::span<const framework::core::CreditSrc> credits, std::vector<AssetInput>& out,
             std::string& error) {
    std::vector<uint8_t> table;
    if (!framework::core::bake_credits(credits, table, error)) return false;
    push_table(name, std::move(table), out);
    return true;
}

} // namespace asset::bakers
