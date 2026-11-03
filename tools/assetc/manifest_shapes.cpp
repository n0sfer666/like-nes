#include "manifest_shapes.hpp"

#include "strings_format.hpp"
#include "manifest_music.hpp"

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

bool strings(const std::vector<std::string>& f, uint32_t line, Error& err) {
    if (f.size() != 5 || f[2] != "tsv")
        return fail(err, line, "strings record expected as strings|<name>|tsv|<path>.tsv|<font>");
    if (!f[3].ends_with(".tsv")) return fail(err, line, "strings path '" + f[3] + "' must end in .tsv");
    if (!name_ok(f[4]) || f[4].size() > framework::ui::MAX_STRING_KEY)
        return fail(err, line, "invalid strings font name '" + f[4] + "'");
    return true;
}

constexpr RecordShape BUILTIN[] = {{"texture", texture}, {"level", level}, {"clips", clips},
                               {"font", font},       {"credits", credits}, {"touch", touch}, {"strings", strings}, {"music", music_shape}};

}
std::span<const RecordShape> builtin_shapes() { return BUILTIN; }
}
