#include "manifest_bake.hpp"

#include <map>

#include "assetc_manifest.hpp"
#include "baker_guid.hpp"
#include "bakers.hpp"
#include "clip_import.hpp"
#include "font_import.hpp"
#include "layer_cover.hpp"
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
    if (r.viewport && !framework::graphics::check_layer_cover(lv.visual, lv.objects, why))
        return fail(b, r.line, file + ": " + why);
    out.push_back(std::move(lv));
    return true;
}

bool clips(Bake& b, const Record& r, const std::string& file, LevelSource& src,
           std::vector<framework::graphics::ClipSrc>& out) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(file, bytes, MAX_LEVEL_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 64 MiB)");
    std::vector<framework::graphics::ClipSrc> got;
    std::string why;
    const bool ok =
        r.codec == "aseprite"
            ? framework::graphics::import_aseprite(r.name, file, std::as_bytes(std::span<const uint8_t>(bytes)), src,
                                                   got, why)
            : framework::graphics::import_sheet(r.name, file, std::string(bytes.begin(), bytes.end()), src, got, why);
    if (!ok) return fail(b, r.line, why);
    if (!framework::graphics::check_clips(got, why)) return fail(b, r.line, file + ": " + why);
    out.insert(out.end(), std::make_move_iterator(got.begin()), std::make_move_iterator(got.end()));
    return true;
}

bool font(Bake& b, const Record& r, const std::string& file, std::vector<framework::graphics::FontSrc>& out) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(file, bytes, MAX_LEVEL_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 64 MiB)");
    framework::graphics::FontAtlas atlas;
    std::string why;
    if (!framework::graphics::import_bitmask_font(r.name, file, std::as_bytes(std::span<const uint8_t>(bytes)), atlas,
                                                  why))
        return fail(b, r.line, why);
    atlas.font.texture_guid = bakers::guid_of(r.name.c_str());
    bakers::rgba_texture(std::move(atlas.rgba), atlas.font.page_w, atlas.font.page_h, r.name.c_str(), b.assets);
    out.push_back(std::move(atlas.font));
    return true;
}

bool credits(Bake& b, const Record& r, const std::string& file) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(file, bytes, MAX_MANIFEST_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 1 MiB)");
    std::vector<framework::core::CreditSrc> src;
    std::string why;
    if (!framework::core::parse_credits(std::string(bytes.begin(), bytes.end()), file, src, why) ||
        !bakers::credits(r.name.c_str(), src, b.assets, why))
        return fail(b, r.line, why);
    return true;
}

struct Gathered {
    std::vector<framework::tiled::Level> levels;
    std::vector<framework::graphics::ClipSrc> clips;
    std::vector<framework::graphics::FontSrc> fonts;
    std::vector<std::pair<Record, std::string>> fighters;
    uint32_t last_level = 0, last_clips = 0, last_font = 0;
};

bool fighter(Bake& b, const Record& r, const std::string& file, std::span<const framework::graphics::ClipSrc> clips) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(file, bytes, MAX_MANIFEST_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 1 MiB)");
    framework::brawl::FighterBakeError err;
    if (bakers::fighter(r.name.c_str(), std::string(bytes.begin(), bytes.end()), clips, b.assets, err)) return true;
    const std::string at = err.line > 0 ? ":" + std::to_string(err.line) : "";
    return fail(b, r.line, file + at + ": " + err.message);
}

bool second_pass(Bake& b, const Record& r, const std::string& file, LevelSource& src, Gathered& g) {
    if (r.kind == "credits") return credits(b, r, file);
    if (r.kind == "fighter") {
        g.fighters.emplace_back(r, file);
        return true;
    }
    if (r.kind == "level") {
        g.last_level = r.line;
        return level(b, r, file, src, g.levels);
    }
    if (r.kind == "clips") {
        g.last_clips = r.line;
        return clips(b, r, file, src, g.clips);
    }
    g.last_font = r.line;
    return font(b, r, file, g.fonts);
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
    Gathered g;
    for (const bool textures_pass : {true, false}) {
        for (const Record& r : records) {
            if ((r.kind == "texture") != textures_pass) continue;
            std::string file, why;
            if (!resolve(base, r.path, file, why)) return fail(b, r.line, why);
            b.deps.push_back(file);
            if (textures_pass ? !texture(b, r, file, textures) : !second_pass(b, r, file, src, g)) return false;
        }
    }
    std::string why;
    if (!g.levels.empty() && !bakers::levels(g.levels, b.assets, why)) return fail(b, g.last_level, why);
    if (!g.clips.empty() && !bakers::clips(g.clips, b.assets, why)) return fail(b, g.last_clips, why);
    if (!g.fonts.empty() && !bakers::fonts(g.fonts, b.assets, why)) return fail(b, g.last_font, why);
    for (const auto& [r, file] : g.fighters)
        if (!fighter(b, r, file, g.clips)) return false;
    return true;
}

} // namespace asset::manifest
