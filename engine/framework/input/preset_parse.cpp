#include "preset_parse.hpp"

#include <cstring>

#include "source_names.hpp"

// Грамматика строки манифеста: сколько полей, что в них лежит и куда это класть. Всё, что нельзя
// решить по одной строке, спрашивается у `preset_validate.cpp`; всё, что про текст как таковой, —
// у `preset_text.cpp`.
namespace framework::input {
namespace {

BindingRow to_row(const ::input::Source& s) {
    BindingRow r;
    r.kind = static_cast<uint32_t>(s.kind);
    r.code = s.code;
    r.sign = s.sign;
    r.reserved = 0;
    return r;
}

bool parse_small_int(const std::string& s, uint32_t& out) {
    if (s.empty() || s.size() > 2) return false;
    uint32_t v = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<uint32_t>(c - '0');
    }
    out = v;
    return true;
}

bool need_preset(const PresetBuild& b, PresetBakeError& err, int line, const char* what) {
    if (!b.presets.empty()) return true;
    return preset_fail(err, line, std::string(what) + " before any preset");
}

bool parse_preset(PresetBuild& b, const std::vector<std::string>& f, int line, PresetBakeError& err) {
    if (f.size() != 2 || f[1].empty()) return preset_fail(err, line, "preset needs exactly one name");
    if (!preset_close(b, err, line)) return false;
    PresetRow p{};
    p.name_offset = b.blob.add(f[1]);
    p.action_begin = static_cast<uint32_t>(b.actions.size());
    p.axis_begin = static_cast<uint32_t>(b.axes.size());
    b.presets.push_back(p);
    return true;
}

bool parse_action(PresetBuild& b, const std::vector<std::string>& f, int line, PresetBakeError& err) {
    if (!need_preset(b, err, line, "action")) return false;
    if (f.size() < 3) return preset_fail(err, line, "action needs a name and at least one source");
    // Действие объявляется один раз со всеми источниками сразу: биндинги действия обязаны лежать
    // непрерывным диапазоном, а вторая строка с тем же именем разорвала бы его молча.
    const PresetRow& cur = b.presets.back();
    for (uint32_t i = 0; i < cur.action_count; ++i)
        if (std::strcmp(b.blob.data.data() + b.actions[cur.action_begin + i].name_offset,
                        f[1].c_str()) == 0)
            return preset_fail(err, line, "action '" + f[1] + "' is already declared in this preset; "
                                   "list every source in one row");
    ActionRow a{};
    a.name_offset = b.blob.add(f[1]);
    a.binding_begin = static_cast<uint32_t>(b.bindings.size());
    for (std::size_t i = 2; i < f.size(); ++i) {
        ::input::Source src;
        if (!parse_source(f[i], src)) return preset_fail(err, line, "unknown source '" + f[i] + "'");
        b.bindings.push_back(to_row(src));
    }
    a.binding_count = static_cast<uint32_t>(b.bindings.size()) - a.binding_begin;
    b.actions.push_back(a);
    ++b.presets.back().action_count;
    return preset_check_action_row(b, line, err);
}

bool parse_axis(PresetBuild& b, const std::vector<std::string>& f, int line, PresetBakeError& err) {
    if (!need_preset(b, err, line, "axis")) return false;
    if (f.size() != 4)
        return preset_fail(err, line, "axis needs a name, a positive source and a negative source ('-' for none)");
    ::input::Source pos{}, neg{};
    if (!parse_source(f[2], pos)) return preset_fail(err, line, "unknown source '" + f[2] + "'");
    if (f[3] != "-" && !parse_source(f[3], neg)) return preset_fail(err, line, "unknown source '" + f[3] + "'");
    AxisRow ax{};
    ax.name_offset = b.blob.add(f[1]);
    ax.pos = to_row(pos);
    ax.neg = to_row(neg);
    ax.deadzone_raw = 0;
    ax.outer_raw = fix32::ONE;
    ax.curve_exp = 1;
    ax.pair_axis = NO_PAIR;
    b.axes.push_back(ax);
    b.axis_names.push_back(f[1]);
    ++b.presets.back().axis_count;
    return preset_check_axis_row(b, f[1], line, err);
}

bool parse_shape(PresetBuild& b, const std::vector<std::string>& f, int line, PresetBakeError& err) {
    if (!need_preset(b, err, line, "shape")) return false;
    if (f.size() != 6)
        return preset_fail(err, line,
                    "shape needs a name, a deadzone, an outer edge, a curve and a pair ('-' for none)");
    fix32 dz, outer;
    if (!preset_parse_fix(f[2], dz) || !preset_parse_fix(f[3], outer))
        return preset_fail(err, line, "deadzone and outer edge must be decimal numbers");
    PresetShape s;
    s.deadzone_raw = dz.raw;
    s.outer_raw = outer.raw;
    if (!parse_small_int(f[4], s.curve_exp) || s.curve_exp < 1 || s.curve_exp > 4)
        return preset_fail(err, line, "curve exponent must be 1..4");
    if (f[5] != "-") s.pair = f[5];
    s.line = line;
    // Форма адресуется именем, поэтому вторая строка не дополняет первую, а затирает её целиком —
    // ровно так же, как вторая строка `action`, которую пекарь отбивает с самого начала.
    if (b.shapes.find(f[1]) != b.shapes.end())
        return preset_fail(err, line, "shape '" + f[1] + "' is already declared in this preset");
    b.shapes[f[1]] = s;
    return true;
}

} // namespace

bool preset_parse_line(PresetBuild& b, const std::vector<std::string>& f, int line,
                       PresetBakeError& err) {
    const std::string& kind = f[0];
    if (kind == "preset") return parse_preset(b, f, line, err);
    if (kind == "action") return parse_action(b, f, line, err);
    if (kind == "axis") return parse_axis(b, f, line, err);
    if (kind == "shape") return parse_shape(b, f, line, err);
    if (kind == "pad") return preset_parse_pad(b, f, line, err);
    return preset_fail(err, line, "unknown row kind '" + kind + "'");
}

} // namespace framework::input
