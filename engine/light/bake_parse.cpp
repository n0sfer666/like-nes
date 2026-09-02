#include "bake_spec.hpp"

#include <cmath>

#include "text.hpp"

namespace light {
namespace {

using mat::text::parse_float;
using mat::text::split;
using mat::text::trim;

// Битовые метки заполненных полей: «поле задано дважды» и «поле забыли» обязаны быть отказом, а
// не последним записанным значением и не нулём. Забытая высота у точечного света дала бы N·L = 0,
// то есть чёрный источник — находку, которую видно только глазами и только на кадре.
enum Field : uint32_t {
    F_POS = 1u << 0,
    F_HEIGHT = 1u << 1,
    F_DIR = 1u << 2,
    F_COLOR = 1u << 3,
    F_INTENSITY = 1u << 4,
    F_RADIUS = 1u << 5,
};

constexpr uint32_t POINT_REQUIRED = F_POS | F_HEIGHT | F_COLOR | F_INTENSITY | F_RADIUS;
constexpr uint32_t DIR_REQUIRED = F_DIR | F_COLOR | F_INTENSITY;

bool fail(BakeError& err, int line, const std::string& msg) {
    err.line = line;
    err.message = msg;
    return false;
}

bool numbers(const std::string& s, std::size_t want, float* out, BakeError& err, int line) {
    const std::vector<std::string> parts = split(s, ',');
    if (parts.size() != want)
        return fail(err, line, "value needs " + std::to_string(want) + " number(s)");
    for (std::size_t k = 0; k < parts.size(); ++k)
        if (!parse_float(parts[k], out[k])) return fail(err, line, "not a decimal: " + parts[k]);
    return true;
}

bool duplicate_name(const std::vector<LightSpec>& all, const std::string& name) {
    for (const LightSpec& l : all)
        if (l.name == name) return true;
    return false;
}

const char* missing(uint32_t have, uint32_t want) {
    if (!(have & F_POS) && (want & F_POS)) return "pos";
    if (!(have & F_HEIGHT) && (want & F_HEIGHT)) return "height";
    if (!(have & F_DIR) && (want & F_DIR)) return "dir";
    if (!(have & F_COLOR) && (want & F_COLOR)) return "color";
    if (!(have & F_INTENSITY) && (want & F_INTENSITY)) return "intensity";
    if (!(have & F_RADIUS) && (want & F_RADIUS)) return "radius";
    return nullptr;
}

bool close_light(LightSpec& cur, uint32_t have, BakeError& err) {
    const uint32_t want = cur.kind == Kind::Point ? POINT_REQUIRED : DIR_REQUIRED;
    if (const char* m = missing(have, want))
        return fail(err, cur.line, "light " + cur.name + " has no " + m);
    if (cur.kind != Kind::Directional) return true;
    // Нормализация делается ЗДЕСЬ, а не в кадре: рантайм тогда платил бы корень на источник, а
    // забытая нормализация тускнела бы молча вместо отказа.
    const float len = std::sqrt(cur.dir[0] * cur.dir[0] + cur.dir[1] * cur.dir[1]);
    if (len < 1e-6f) return fail(err, cur.line, "direction of " + cur.name + " has no length");
    cur.dir[0] /= len;
    cur.dir[1] /= len;
    return true;
}

} // namespace

bool parse_lights(const std::string& text, LightSet& set, BakeError& err) {
    std::vector<LightSpec>& out = set.lights;
    out.clear();
    set.ambient[0] = set.ambient[1] = set.ambient[2] = set.ambient[3] = 0;
    bool ambient_set = false;
    uint32_t have = 0;
    int line_no = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string raw = text.substr(pos, nl == std::string::npos ? nl : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        ++line_no;

        const std::size_t hash = raw.find('#');
        const std::string line = trim(hash == std::string::npos ? raw : raw.substr(0, hash));
        if (line.empty()) continue;

        const std::vector<std::string> f = split(line, '|');

        // Ambient разбирается ДО проверки «строка вне света»: он и есть строка вне света, и
        // ставить его можно один раз — заданный дважды, он молча взял бы последнее значение.
        if (f[0] == "ambient") {
            if (f.size() != 3) return fail(err, line_no, "ambient | r, g, b | strength");
            if (ambient_set) return fail(err, line_no, "ambient is set twice");
            if (!numbers(f[1], 3, set.ambient, err, line_no)) return false;
            if (!numbers(f[2], 1, &set.ambient[3], err, line_no)) return false;
            for (int k = 0; k < 4; ++k)
                if (set.ambient[k] < 0) return fail(err, line_no, "ambient is negative");
            ambient_set = true;
            continue;
        }

        if (f[0] == "light") {
            if (!out.empty() && !close_light(out.back(), have, err)) return false;
            if (f.size() != 3) return fail(err, line_no, "light | name | point|directional");
            if (f[1].empty()) return fail(err, line_no, "empty light name");
            if (duplicate_name(out, f[1])) return fail(err, line_no, "duplicate light " + f[1]);
            LightSpec l;
            l.name = f[1];
            l.line = line_no;
            if (f[2] == "point") l.kind = Kind::Point;
            else if (f[2] == "directional") l.kind = Kind::Directional;
            else return fail(err, line_no, "kind is point or directional");
            out.push_back(l);
            have = 0;
            continue;
        }

        if (out.empty()) return fail(err, line_no, "row before any light");
        LightSpec& cur = out.back();
        if (f[0] != "set") return fail(err, line_no, "unknown row " + f[0]);
        if (f.size() != 3) return fail(err, line_no, "set | field | value");

        const std::string& name = f[1];
        const bool point = cur.kind == Kind::Point;
        uint32_t bit = 0;
        if (name == "pos") { bit = F_POS; if (!point) return fail(err, line_no, "pos is for a point light"); }
        else if (name == "height") { bit = F_HEIGHT; if (!point) return fail(err, line_no, "height is for a point light"); }
        else if (name == "radius") { bit = F_RADIUS; if (!point) return fail(err, line_no, "radius is for a point light"); }
        else if (name == "dir") { bit = F_DIR; if (point) return fail(err, line_no, "dir is for a directional light"); }
        else if (name == "color") bit = F_COLOR;
        else if (name == "intensity") bit = F_INTENSITY;
        else return fail(err, line_no, "unknown field " + name);
        if (have & bit) return fail(err, line_no, "field " + name + " is set twice");
        have |= bit;

        if (bit == F_POS && !numbers(f[2], 2, cur.pos, err, line_no)) return false;
        if (bit == F_DIR && !numbers(f[2], 2, cur.dir, err, line_no)) return false;
        if (bit == F_COLOR && !numbers(f[2], 3, cur.color, err, line_no)) return false;
        if (bit == F_HEIGHT && !numbers(f[2], 1, &cur.height, err, line_no)) return false;
        if (bit == F_INTENSITY && !numbers(f[2], 1, &cur.intensity, err, line_no)) return false;
        if (bit == F_RADIUS && !numbers(f[2], 1, &cur.radius, err, line_no)) return false;

        if (bit == F_COLOR && (cur.color[0] < 0 || cur.color[1] < 0 || cur.color[2] < 0))
            return fail(err, line_no, "colour channel is negative");
        if (bit == F_INTENSITY && cur.intensity < 0) return fail(err, line_no, "intensity is negative");
        if (bit == F_RADIUS && cur.radius <= 0) return fail(err, line_no, "radius must be positive");
    }
    if (out.empty()) return fail(err, 0, "no lights in the source");
    // Последний свет закрывается ПЕРВЫМ: у его отказа есть номер строки, а у «нет ambient» — нет,
    // и назвать источник поимённо ценнее, чем сказать про набор.
    if (!close_light(out.back(), have, err)) return false;
    if (!ambient_set) return fail(err, 0, "no ambient in the source");
    return true;
}

} // namespace light
