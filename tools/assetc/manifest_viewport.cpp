#include "manifest_viewport.hpp"

#include "assetc_manifest.hpp"
#include "bakers.hpp"
#include "manifest_bake.hpp"
#include "text_fields.hpp"

namespace asset::manifest {

bool viewport_key(const std::string& key) { return key == "zone" || key == "zone_limit"; }

bool read_viewport(const std::vector<std::string>& f, uint32_t line,
                   framework::graphics::ViewportSettings& out, ViewportLines& seen, Error& error) {
    auto fail = [&](const std::string& why) { error = {line, why}; return false; };
    if (f.size() != 3) return fail(f[0] + " expected as " + f[0] + "|<width>|<height>");
    uint32_t& first = f[0] == "zone" ? seen.zone : seen.limit;
    if (first != 0) return fail("duplicate " + f[0] + " (first on line " + std::to_string(first) + ")");
    uint32_t w = 0, h = 0;
    if (!framework::core::parse_u32(f[1], w) || !framework::core::parse_u32(f[2], h) ||
        w == 0 || h == 0 || w > framework::graphics::VIEWPORT_SIDE_MAX || h > framework::graphics::VIEWPORT_SIDE_MAX)
        return fail(f[0] + " sides must be integers between 1 and " +
                    std::to_string(framework::graphics::VIEWPORT_SIDE_MAX));
    (f[0] == "zone" ? out.zone : out.limit) = {w, h};
    first = line;
    return true;
}

bool bake_viewport(Bake& b) {
    if (framework::graphics::viewport_settings_default(b.viewport)) return true;
    std::vector<uint8_t> table;
    std::string why;
    if (!framework::graphics::bake_viewport_settings(b.viewport, table, why)) return bake_fail(b, 0, why);
    bakers::push_table("viewport", std::move(table), b.assets);
    return true;
}

} // namespace asset::manifest
