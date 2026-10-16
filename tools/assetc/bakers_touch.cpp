#include "bakers.hpp"

namespace asset::bakers {

bool touch(const char* name, const framework::touch::Layout& layout, std::vector<AssetInput>& out,
           std::string& error) {
    std::vector<uint8_t> table;
    if (!framework::touch::bake_layout(layout, table, error)) return false;
    push_table(name, std::move(table), out);
    return true;
}

} // namespace asset::bakers
