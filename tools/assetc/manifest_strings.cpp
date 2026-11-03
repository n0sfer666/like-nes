#include "manifest_strings.hpp"

#include <algorithm>
#include <cstdio>

#include "bakers.hpp"
#include "platform_fs.hpp"
#include "strings_bake.hpp"
#include "strings_format.hpp"

namespace asset::manifest {
bool bake_strings(Bake& b, const StringsRecord& source, std::span<const framework::graphics::FontSrc> fonts) {
    const auto& r = source.record;
    const auto font = std::find_if(fonts.begin(), fonts.end(), [&](const auto& f) { return f.name == r.font; });
    if (font == fonts.end()) return bake_fail(b, r.line, "strings '" + r.name + "' refers to missing font '" + r.font + "'");
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(source.file, bytes, framework::ui::MAX_STRINGS_BYTES))
        return bake_fail(b, r.line, "cannot read " + source.file + " (missing, unreadable or over 16 MiB)");
    std::vector<framework::ui::StringSrc> rows;
    framework::ui::StringsStats stats;
    std::string why;
    const auto* data = bytes.empty() ? "" : reinterpret_cast<const char*>(bytes.data());
    if (!framework::ui::parse_strings_tsv(std::string_view(data, bytes.size()), source.file, rows, why))
        return bake_fail(b, r.line, why);
    if (!framework::ui::bake_strings(rows, *font, b.strict, bytes, stats, why))
        return bake_fail(b, r.line, source.file + ": " + why);
    bakers::push_table(r.name.c_str(), std::move(bytes), b.assets);
    std::printf("[assetc] strings %s: keys=%u fallbacks=%u\n", r.name.c_str(), stats.keys, stats.fallbacks);
    return true;
}
}
