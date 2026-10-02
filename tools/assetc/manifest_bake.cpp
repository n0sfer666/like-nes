#include "manifest_bake.hpp"

#include <map>

#include "assetc_manifest.hpp"
#include "baker_guid.hpp"
#include "bakers.hpp"
#include "level_source.hpp"
#include "platform_fs.hpp"

namespace asset::manifest {
namespace {

// Потолки чтения — до разбора: PNG 2048² RGBA8 в stored-блоках чуть больше 16 МиБ, манифест —
// строки текста. Файл крупнее — не ассет, а чужое (образ, /dev/zero через ссылку).
constexpr size_t MAX_PNG_BYTES = 64u << 20;
constexpr size_t MAX_MANIFEST_BYTES = 1u << 20;
constexpr size_t MAX_LEVEL_BYTES = 64u << 20;

std::string dir_of(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return slash == 0 ? path.substr(0, 1) : path.substr(0, slash);
}

bool fail(Bake& b, uint32_t line, const std::string& message) {
    b.error = b.manifest + ":" + std::to_string(line) + ": " + message;
    return false;
}

using Textures = std::map<std::string, framework::tiled::Image>;

bool texture(Bake& b, const Record& r, const std::string& file, Textures& textures) {
    std::vector<uint8_t> png;
    if (!platform::read_bytes_capped(file, png, MAX_PNG_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 64 MiB)");
    std::string why;
    uint32_t w = 0, h = 0;
    if (!codec::png_info(png, w, h, why)) return fail(b, r.line, file + ": " + why);
    textures[r.path] = framework::tiled::Image{bakers::guid_of(r.name.c_str()), w, h};
    if (r.codec == "pixel") {
        if (!bakers::pixel(png, r.name.c_str(), b.assets, why)) return fail(b, r.line, file + ": " + why);
        return true;
    }
    if (!bakers::texture(b.tools, file, r.name.c_str(), b.tmp, b.assets))
        return fail(b, r.line, file + ": hd bake failed (basisu missing or it failed, see above); "
                               "pixel needs no basisu");
    return true;
}

bool level(Bake& b, const Record& r, const std::string& file, LevelSource& src,
           std::vector<framework::tiled::Level>& out) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(file, bytes, MAX_LEVEL_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 64 MiB)");
    framework::tiled::Level lv;
    std::string why;
    if (!framework::tiled::import_tmj(r.name, file, std::as_bytes(std::span<const uint8_t>(bytes)), src, lv, why))
        return fail(b, r.line, why);
    out.push_back(std::move(lv));
    return true;
}

} // namespace

bool bake(Bake& b) {
    b.assets.clear();
    b.deps.clear();
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(b.manifest, bytes, MAX_MANIFEST_BYTES)) {
        b.error = b.manifest + ": cannot read the manifest (missing, unreadable or over 1 MiB)";
        return false;
    }
    const std::string text(bytes.begin(), bytes.end());
    b.deps.push_back(b.manifest);
    std::vector<Record> records;
    Error err;
    if (!parse(text, records, err)) return fail(b, err.line, err.message);
    const std::string base = dir_of(b.manifest);
    Textures textures;
    LevelSource src(base, textures, b.deps);
    std::vector<framework::tiled::Level> levels;
    uint32_t last_level = 0;
    for (const bool textures_pass : {true, false}) {
        for (const Record& r : records) {
            if ((r.kind == "texture") != textures_pass) continue;
            std::string file, why;
            if (!resolve(base, r.path, file, why)) return fail(b, r.line, why);
            b.deps.push_back(file);
            if (textures_pass ? !texture(b, r, file, textures) : !level(b, r, file, src, levels)) return false;
            last_level = textures_pass ? last_level : r.line;
        }
    }
    std::string why;
    if (!levels.empty() && !bakers::levels(levels, b.assets, why)) return fail(b, last_level, why);
    return true;
}

} // namespace asset::manifest
