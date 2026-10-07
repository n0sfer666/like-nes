#include "assetc_manifest.hpp"

#include <map>

#include "text_fields.hpp"

namespace asset::manifest {
namespace {

bool name_ok(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

// `..` отбивается целиком, а не только выводящий наружу: запись с ним всегда можно написать
// прямо, а лексическая проверка «внутри ли» — второе правило, в котором можно ошибиться. Ссылки
// не проверяются: манифест пишет автор игры о своём дереве, общие ассеты через ссылку — законный
// приём, и правило держит переносимость записи, а не песочницу. `:` — диск или поток NTFS,
// `\` — разделитель, который POSIX считает частью имени.
bool path_ok(const std::string& path, std::string& error) {
    if (path.empty()) {
        error = "empty path";
        return false;
    }
    if (path.find('\\') != std::string::npos) {
        error = "path '" + path + "' uses '\\'; write paths with '/'";
        return false;
    }
    if (path[0] == '/' || (path.size() > 1 && path[1] == ':')) {
        error = "path '" + path + "' is absolute; paths are relative to the manifest";
        return false;
    }
    if (path.find(':') != std::string::npos) {
        error = "path '" + path + "' uses ':'; Windows reads it as a drive or a stream";
        return false;
    }
    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string seg = path.substr(start, end - start);
        if (seg == "..") {
            error = "path '" + path + "' uses '..'; files must lie under the manifest directory";
            return false;
        }
        if (seg.empty() || seg == ".") {
            error = "path '" + path + "' has an empty or '.' segment";
            return false;
        }
        start = end + 1;
    }
    return true;
}

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

bool fighter(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() != 3) return fail(err, line, "fighter record expected as fighter|<name>|<path>.fighter");
    if (!f[2].ends_with(".fighter")) return fail(err, line, "fighter path '" + f[2] + "' must end in .fighter");
    return true;
}

bool kind_ok(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f[0] == "texture") return texture(f, line, err);
    if (f[0] == "level") return level(f, line, err);
    if (f[0] == "clips") return clips(f, line, err);
    if (f[0] == "font") return font(f, line, err);
    if (f[0] == "credits") return credits(f, line, err);
    if (f[0] == "fighter") return fighter(f, line, err);
    return fail(err, line,
                "unsupported record kind '" + f[0] + "' (this assetc bakes: texture, level, clips, font, credits, fighter)");
}

bool reserved(const std::string& name) {
    return name == "tilemap" || name == "visual" || name == "objects" || name == "clips" || name == "fonts";
}

bool record(const std::vector<std::string>& f, uint32_t line, Record& rec, Error& err) {
    if (!kind_ok(f, line, err)) return false;
    if (!name_ok(f[1]))
        return fail(err, line, "name '" + f[1] + "' must be letters, digits, '_', '-' or '.'");
    if (f[0] != "level" && f[0] != "clips" && reserved(f[1]))
        return fail(err, line, f[0] + " name '" + f[1] + "' is reserved for the level, clip and font sections");
    const bool no_codec = f[0] == "credits" || f[0] == "fighter";
    const std::string& path = no_codec ? f[2] : f[3];
    std::string why;
    if (!path_ok(path, why)) return fail(err, line, why);
    rec = Record{line, f[0], f[1], no_codec ? std::string() : f[2], path, f.size() == 5 && f[4] == "viewport"};
    return true;
}

} // namespace

bool parse(const std::string& text, std::vector<Record>& out, Error& err) {
    out.clear();
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
        if (!record(framework::core::split_fields(body), line, rec, err)) return false;
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
