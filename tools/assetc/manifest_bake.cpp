#include "manifest_bake.hpp"

#include "assetc_manifest.hpp"
#include "bakers.hpp"
#include "platform_fs.hpp"

namespace asset::manifest {
namespace {

// Потолки чтения — до разбора: PNG 2048² RGBA8 в stored-блоках чуть больше 16 МиБ, манифест —
// строки текста. Файл крупнее — не ассет, а чужое (образ, /dev/zero через ссылку).
constexpr size_t MAX_PNG_BYTES = 64u << 20;
constexpr size_t MAX_MANIFEST_BYTES = 1u << 20;

std::string dir_of(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return slash == 0 ? path.substr(0, 1) : path.substr(0, slash);
}

bool fail(Bake& b, uint32_t line, const std::string& message) {
    b.error = b.manifest + ":" + std::to_string(line) + ": " + message;
    return false;
}

bool texture(Bake& b, const Record& r, const std::string& file) {
    std::vector<uint8_t> png;
    if (!platform::read_bytes_capped(file, png, MAX_PNG_BYTES))
        return fail(b, r.line, "cannot read " + file + " (missing, unreadable or over 64 MiB)");
    std::string why;
    if (r.codec == "pixel") {
        if (!bakers::pixel(png, r.name.c_str(), b.assets, why)) return fail(b, r.line, file + ": " + why);
        return true;
    }
    uint32_t w = 0, h = 0;
    if (!codec::png_info(png, w, h, why)) return fail(b, r.line, file + ": " + why);
    if (!bakers::texture(b.tools, file, r.name.c_str(), b.tmp, b.assets))
        return fail(b, r.line, file + ": hd bake failed (basisu missing or it failed, see above); "
                               "pixel needs no basisu");
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
    for (const Record& r : records) {
        std::string file, why;
        if (!resolve(base, r.path, file, why)) return fail(b, r.line, why);
        b.deps.push_back(file);
        if (!texture(b, r, file)) return false;
    }
    return true;
}

} // namespace asset::manifest
