#include "assetc_manifest.hpp"

#include <map>

#include "text_fields.hpp"

namespace asset::manifest {
namespace {

bool fail(Error& err, uint32_t line, std::string message) {
    err.line = line;
    err.message = std::move(message);
    return false;
}

bool texture(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() == 3 && (f[2] == "pixel" || f[2] == "hd"))
        return fail(err, line, "texture record needs a path: texture|<name>|" + f[2] + "|<path>");
    if (f.size() == 3)
        return fail(err, line, "texture record needs a codec: texture|<name>|pixel|<path> or "
                               "texture|<name>|hd|<path>");
    if (f.size() != 4)
        return fail(err, line, "texture record has " + std::to_string(f.size()) +
                                   " fields, expected texture|<name>|<codec>|<path>");
    if (f[2] != "pixel" && f[2] != "hd")
        return fail(err, line, "unknown codec '" + f[2] + "', expected pixel or hd");
    return true;
}

bool level(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if ((f.size() != 4 && f.size() != 5) || f[2] != "tiled")
        return fail(err, line, "level record expected as level|<name>|tiled|<path>.tmj or "
                               "level|<name>|tiled|<path>.tmj|viewport");
    if (f.size() == 5 && f[4] != "viewport")
        return fail(err, line, "unknown level option '" + f[4] + "', expected viewport");
    const std::string& path = f[3];
    if (path.size() < 4 || path.compare(path.size() - 4, 4, ".tmj") != 0)
        return fail(err, line, "level path '" + path + "' must be a Tiled JSON map (.tmj)");
    return true;
}

bool clips(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() != 4 || (f[2] != "aseprite" && f[2] != "sheet"))
        return fail(err, line, "clips record expected as clips|<name>|aseprite|<path>.json or "
                               "clips|<name>|sheet|<path>.sheet");
    const std::string ext = f[2] == "aseprite" ? ".json" : ".sheet";
    if (!f[3].ends_with(ext)) return fail(err, line, "clips path '" + f[3] + "' must end in " + ext);
    return true;
}

bool font(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() != 4 || f[2] != "bitmask")
        return fail(err, line, "font record expected as font|<name>|bitmask|<path>.json");
    if (!f[3].ends_with(".json")) return fail(err, line, "font path '" + f[3] + "' must end in .json");
    return true;
}

bool credits(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() != 3) return fail(err, line, "credits record expected as credits|<name>|<path>.txt");
    if (!f[2].ends_with(".txt")) return fail(err, line, "credits path '" + f[2] + "' must end in .txt");
    return true;
}

bool touch(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() != 4 || f[2] != "layout")
        return fail(err, line, "touch record expected as touch|<name>|layout|<path>.txt");
    if (!f[3].ends_with(".txt")) return fail(err, line, "touch path '" + f[3] + "' must end in .txt");
    return true;
}

bool game(const Kind& k, const std::vector<std::string>& f, uint32_t line, Error& err) {
    const std::string shape = f[0] + "|<name>|<path>" + k.extension;
    if (f.size() != 3) return fail(err, line, f[0] + " record expected as " + shape);
    if (!f[2].ends_with(k.extension))
        return fail(err, line, f[0] + " path '" + f[2] + "' must end in " + k.extension);
    return true;
}

struct Builtin {
    const char* name;
    bool (*check)(const std::vector<std::string>& f, uint32_t line, Error& err);
};
constexpr Builtin BUILTIN[] = {{"texture", texture}, {"level", level}, {"clips", clips},
                               {"font", font},       {"credits", credits}, {"touch", touch}};

bool kind_ok(const std::vector<std::string>& f, uint32_t line, std::span<const Kind> kinds, Error& err) {
    for (const Builtin& b : BUILTIN)
        if (f[0] == b.name) return b.check(f, line, err);
    if (const Kind* k = find_kind(kinds, f[0])) return game(*k, f, line, err);
    std::string known;
    for (const Builtin& b : BUILTIN) known += std::string(known.empty() ? "" : ", ") + b.name;
    for (const Kind& k : kinds) known += std::string(", ") + k.name;
    return fail(err, line, "unsupported record kind '" + f[0] + "' (this tool bakes: " + known + ")");
}

bool table_fail(std::string& error, const std::string& message) {
    error = "kind table: " + message;
    return false;
}

bool reserved(const std::string& name) {
    return name == "tilemap" || name == "visual" || name == "objects" || name == "clips" || name == "fonts";
}

bool record(const std::vector<std::string>& f, uint32_t line, std::span<const Kind> kinds, Record& rec,
            Error& err) {
    if (!kind_ok(f, line, kinds, err)) return false;
    if (!name_ok(f[1]))
        return fail(err, line, "name '" + f[1] + "' must be letters, digits, '_', '-' or '.'");
    if (f[0] != "level" && f[0] != "clips" && reserved(f[1]))
        return fail(err, line, f[0] + " name '" + f[1] + "' is reserved for the level, clip and font sections");
    const bool no_codec = f[0] == "credits" || find_kind(kinds, f[0]) != nullptr;
    const std::string& path = no_codec ? f[2] : f[3];
    std::string why;
    if (!path_ok(path, why)) return fail(err, line, why);
    rec = Record{line, f[0], f[1], no_codec ? std::string() : f[2], path, f.size() == 5 && f[4] == "viewport"};
    return true;
}

} // namespace

// Вид игры с именем встроенного молча проиграл бы встроенному, а второй вид с тем же именем —
// первому (find_kind берёт первый): записи игры пеклись бы чужим пекарем.
bool kinds_ok(std::span<const Kind> kinds, std::string& error) {
    for (size_t i = 0; i < kinds.size(); ++i) {
        const Kind& k = kinds[i];
        if (!k.name || !k.extension || !k.bake)
            return table_fail(error, "kind #" + std::to_string(i) + " has no name, extension or baker");
        const std::string name = k.name, ext = k.extension;
        if (!name_ok(name))
            return table_fail(error, "kind name '" + name + "' must be letters, digits, '_', '-' or '.'");
        if (ext.size() < 2 || ext[0] != '.' || !name_ok(ext.substr(1)))
            return table_fail(error, "kind '" + name + "' extension '" + ext +
                                         "' must be '.' and a suffix of letters, digits, '_', '-' or '.'");
        for (const Builtin& b : BUILTIN)
            if (name == b.name) return table_fail(error, "game record kind '" + name + "' shadows a built-in kind");
        for (size_t j = 0; j < i; ++j)
            if (name == kinds[j].name) return table_fail(error, "game record kind '" + name + "' is listed twice");
    }
    return true;
}

const Kind* find_kind(std::span<const Kind> kinds, const std::string& name) {
    for (const Kind& k : kinds)
        if (name == k.name) return &k;
    return nullptr;
}

bool parse(const std::string& text, std::vector<Record>& out, Error& err, std::span<const Kind> kinds) {
    out.clear();
    if (std::string why; !kinds_ok(kinds, why)) return fail(err, 0, why);
    // BOM пишет PowerShell 5.1 (`Set-Content -Encoding UTF8`); без отдельного отказа он прилип бы
    // к первому полю и дал «unsupported record kind 'texture'» с невидимыми байтами.
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0)
        return fail(err, 1, "manifest starts with a UTF-8 BOM; save the file as UTF-8 without BOM");
    std::map<std::string, uint32_t> seen;
    uint32_t line = 0;
    size_t start = 0;
    while (start < text.size()) {
        const size_t nl = text.find('\n', start);
        const size_t end = nl == std::string::npos ? text.size() : nl;
        std::string body = text.substr(start, end - start);
        start = end + 1;
        ++line;
        const size_t hash = body.find('#');
        if (hash != std::string::npos) body.resize(hash);
        if (framework::core::trim(body).empty()) continue;
        Record rec;
        if (!record(framework::core::split_fields(body), line, kinds, rec, err)) return false;
        const auto [it, fresh] = seen.emplace(rec.name, line);
        if (!fresh)
            return fail(err, line, "duplicate name '" + rec.name + "' (first on line " +
                                       std::to_string(it->second) + ")");
        out.push_back(std::move(rec));
    }
    if (out.empty()) return fail(err, 0, "manifest has no records");
    return true;
}

} // namespace asset::manifest
