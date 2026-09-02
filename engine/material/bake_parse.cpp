#include "bake_rows.hpp"

#include "text.hpp"

namespace mat {
namespace {

using text::parse_float;
using text::parse_u8;
using text::split;
using text::trim;

bool fail(BakeError& err, int line, const std::string& msg) {
    err.line = line;
    err.message = msg;
    return false;
}

bool parse_blend(const std::string& s, Blend& out) {
    if (s == "opaque") { out = Blend::Opaque; return true; }
    if (s == "alpha") { out = Blend::Alpha; return true; }
    if (s == "additive") { out = Blend::Additive; return true; }
    return false;
}

bool parse_values(const std::string& s, ParamType type, float value[4], BakeError& err, int line) {
    const std::vector<std::string> parts = split(s, ',');
    const uint32_t want = param_floats(type);
    if (parts.size() != want)
        return fail(err, line, "value needs " + std::to_string(want) + " numbers");
    for (std::size_t k = 0; k < parts.size(); ++k)
        if (!parse_float(parts[k], value[k])) return fail(err, line, "not a decimal: " + parts[k]);
    return true;
}

int find_material(const std::vector<MaterialSpec>& all, const std::string& name) {
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].name == name) return static_cast<int>(i);
    return -1;
}

// Занятые слоты блока — по всей цепочке баз: инстанс, объявляющий СВОЙ параметр, не имеет права
// сесть на слот базы, иначе наследование затирало бы чужое значение.
uint32_t used_slots(const std::vector<MaterialSpec>& all, int at) {
    uint32_t used = 0;
    while (at >= 0) {
        for (const ParamSpec& p : all[static_cast<std::size_t>(at)].params)
            used |= ((1u << param_floats(p.type)) - 1u) << p.slot;
        at = all[static_cast<std::size_t>(at)].base;
    }
    return used;
}

bool alloc_slot(uint32_t used, uint32_t floats, uint8_t& out) {
    const uint32_t mask = (1u << floats) - 1u;
    for (uint32_t s = 0; s + floats <= PARAM_BLOCK_FLOATS; ++s)
        if ((used & (mask << s)) == 0) { out = static_cast<uint8_t>(s); return true; }
    return false;
}

const ParamSpec* inherited(const std::vector<MaterialSpec>& all, int at, const std::string& name) {
    while (at >= 0) {
        for (const ParamSpec& p : all[static_cast<std::size_t>(at)].params)
            if (p.name == name) return &p;
        at = all[static_cast<std::size_t>(at)].base;
    }
    return nullptr;
}

} // namespace

bool parse_materials(const std::string& text, std::vector<MaterialSpec>& out, BakeError& err) {
    out.clear();
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
        MaterialSpec* cur = out.empty() ? nullptr : &out.back();

        if (f[0] == "material" || f[0] == "instance") {
            const bool inst = f[0] == "instance";
            if (f.size() != (inst ? 3u : 4u))
                return fail(err, line_no,
                            inst ? "instance | name | base" : "material | name | shader | blend");
            if (f[1].empty()) return fail(err, line_no, "empty material name");
            if (find_material(out, f[1]) >= 0) return fail(err, line_no, "duplicate material " + f[1]);
            MaterialSpec m;
            m.name = f[1];
            m.line = line_no;
            if (inst) {
                m.base = find_material(out, f[2]);
                if (m.base < 0) return fail(err, line_no, "unknown base " + f[2]);
                m.shader = out[static_cast<std::size_t>(m.base)].shader;
                m.blend = out[static_cast<std::size_t>(m.base)].blend;
            } else {
                if (f[2].empty()) return fail(err, line_no, "empty shader name");
                m.shader = f[2];
                if (!parse_blend(f[3], m.blend))
                    return fail(err, line_no, "blend is opaque, alpha or additive");
            }
            out.push_back(m);
            continue;
        }

        if (cur == nullptr) return fail(err, line_no, "row before any material");

        if (f[0] == "param") {
            if (f.size() != 5) return fail(err, line_no, "param | name | type | unit | value");
            ParamSpec p;
            p.name = f[1];
            p.line = line_no;
            if (p.name.empty()) return fail(err, line_no, "empty parameter name");
            if (inherited(out, cur->base, p.name) != nullptr)
                return fail(err, line_no, "parameter " + p.name + " is already in the base");
            for (const ParamSpec& q : cur->params)
                if (q.name == p.name) return fail(err, line_no, "duplicate parameter " + p.name);
            if (!param_type_from_name(f[2].c_str(), p.type))
                return fail(err, line_no, "type is scalar, vec2, vec4 or color");
            if (!unit_from_name(f[3].c_str(), p.unit))
                return fail(err, line_no, "unit is raw, fraction, pixels or degrees");
            if (!parse_values(f[4], p.type, p.value, err, line_no)) return false;
            for (uint32_t k = 0; k < param_floats(p.type); ++k) p.value[k] = to_raw(p.unit, p.value[k]);
            if (!alloc_slot(used_slots(out, static_cast<int>(out.size()) - 1), param_floats(p.type),
                            p.slot))
                return fail(err, line_no, "instance parameter block is full");
            if (cur->params.size() >= MAX_PARAMS) return fail(err, line_no, "too many parameters");
            cur->params.push_back(p);
            continue;
        }

        if (f[0] == "set") {
            if (f.size() != 3) return fail(err, line_no, "set | name | value");
            const ParamSpec* base = inherited(out, cur->base, f[1]);
            if (base == nullptr) return fail(err, line_no, "no inherited parameter " + f[1]);
            for (const ParamSpec& q : cur->params)
                if (q.name == f[1]) return fail(err, line_no, "duplicate override " + f[1]);
            ParamSpec p = *base;
            p.line = line_no;
            if (!parse_values(f[2], p.type, p.value, err, line_no)) return false;
            for (uint32_t k = 0; k < param_floats(p.type); ++k) p.value[k] = to_raw(p.unit, p.value[k]);
            cur->params.push_back(p);
            continue;
        }

        if (f[0] == "tex") {
            if (f.size() != 4) return fail(err, line_no, "tex | name | asset | binding");
            TexSpec t;
            t.name = f[1];
            t.asset = f[2];
            t.line = line_no;
            if (t.name.empty() || t.asset.empty()) return fail(err, line_no, "empty texture field");
            if (!parse_u8(f[3], t.binding) || t.binding >= MAX_TEXTURES)
                return fail(err, line_no, "binding is 0.." + std::to_string(MAX_TEXTURES - 1));
            for (const TexSpec& q : cur->textures)
                if (q.binding == t.binding) return fail(err, line_no, "binding is already taken");
            if (cur->textures.size() >= MAX_TEXTURES) return fail(err, line_no, "too many textures");
            cur->textures.push_back(t);
            continue;
        }

        return fail(err, line_no, "unknown row " + f[0]);
    }
    if (out.empty()) return fail(err, 0, "no materials in the source");
    return true;
}

} // namespace mat
