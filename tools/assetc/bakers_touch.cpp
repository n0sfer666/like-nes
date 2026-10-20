#include "bakers.hpp"

#include <cstdio>

#include "platform_fs.hpp"

namespace asset::bakers {

bool touch(const char* name, const framework::touch::Layout& layout, std::vector<AssetInput>& out,
           std::string& error) {
    std::vector<uint8_t> table;
    if (!framework::touch::bake_layout(layout, table, error)) return false;
    push_table(name, std::move(table), out);
    return true;
}

bool touch_layout(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(src, bytes, 1u << 20)) {
        std::fprintf(stderr, "[assetc] touch %s: missing, unreadable or over 1 MiB\n", src.c_str());
        return false;
    }
    framework::touch::Layout layout;
    std::string error;
    if (framework::touch::parse_layout(std::string(bytes.begin(), bytes.end()), src, layout, error) &&
        touch("touch", layout, out, error))
        return true;
    std::fprintf(stderr, "[assetc] touch: %s\n", error.c_str());
    return false;
}

} // namespace asset::bakers
