#include "preset_parse.hpp"

#include <cstring>

#include "source_names.hpp"

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
    return true;
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
    return true;
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
    b.shapes[f[1]] = s;
    return true;
}

} // namespace

// Дробное значение из манифеста — единственное место, где fix32 берётся из текста. Разбор свой,
// а не strtod: `strtod` зависит от локали, и в ru_RU запятая с точкой меняются местами, отчего
// «0.18» стало бы нулём на машине автора и мёртвой зоной на машине сборщика.
bool preset_parse_fix(const std::string& s, fix32& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    int32_t sign = 1;
    if (s[i] == '-') { sign = -1; ++i; }
    int64_t whole = 0;
    bool any = false;
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) {
        whole = whole * 10 + (s[i] - '0');
        if (whole > 32767) return false;
        any = true;
    }
    int64_t frac_raw = 0;
    if (i < s.size() && s[i] == '.') {
        ++i;
        int64_t scale = 1, frac = 0;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) {
            if (scale > 1000000) continue;   // разрядность за пределами Q16.16 отбрасывается
            frac = frac * 10 + (s[i] - '0');
            scale *= 10;
            any = true;
        }
        frac_raw = (frac * fix32::ONE + scale / 2) / scale;
    }
    if (!any || i != s.size()) return false;
    out = fix32::from_raw(static_cast<int32_t>(sign * (whole * fix32::ONE + frac_raw)));
    return true;
}

bool preset_fail(PresetBakeError& err, int line, const std::string& message) {
    err.line = line;
    err.message = message;
    return false;
}

uint32_t PresetStrings::add(const std::string& s) {
    if (s.empty()) return 0;
    const auto it = seen.find(s);
    if (it != seen.end()) return it->second;
    const uint32_t off = static_cast<uint32_t>(data.size());
    data.insert(data.end(), s.begin(), s.end());
    data.push_back('\0');
    seen.emplace(s, off);
    return off;
}

std::string preset_trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> preset_split(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = line.find('|', start);
        if (bar == std::string::npos) { out.push_back(preset_trim(line.substr(start))); break; }
        out.push_back(preset_trim(line.substr(start, bar - start)));
        start = bar + 1;
    }
    return out;
}

bool preset_close(PresetBuild& b, PresetBakeError& err, int line) {
    if (b.presets.empty()) return true;
    PresetRow& p = b.presets.back();
    for (uint32_t i = p.axis_begin; i < p.axis_begin + p.axis_count; ++i) {
        const auto it = b.shapes.find(b.axis_names[i]);
        if (it == b.shapes.end()) continue;
        b.axes[i].deadzone_raw = it->second.deadzone_raw;
        b.axes[i].outer_raw = it->second.outer_raw;
        b.axes[i].curve_exp = it->second.curve_exp;
        if (it->second.pair.empty()) continue;
        // Пара хранится ЛОГИЧЕСКИМ номером оси, а не номером строки: одна ось объявляется
        // несколькими строками (клавиши, стрелки, стик), и номер строки указывал бы на
        // альтернативный биндинг вместо самой оси.
        uint32_t pair = NO_PAIR, logical = 0;
        for (uint32_t j = p.axis_begin; j < p.axis_begin + p.axis_count; ++j) {
            if (b.axis_names[j] == it->second.pair) { pair = logical; break; }
            bool first = true;
            for (uint32_t k = p.axis_begin; k < j; ++k)
                if (b.axis_names[k] == b.axis_names[j]) { first = false; break; }
            if (first) ++logical;
        }
        if (pair == NO_PAIR)
            return preset_fail(err, line,
                        "shape pairs axis '" + it->second.pair + "' which the preset does not declare");
        b.axes[i].pair_axis = pair;
    }
    b.shapes.clear();
    return true;
}

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
