#include <string>
#include <vector>

#include "input_types.hpp"
#include "preset_parse.hpp"
#include "source_names.hpp"

// Проверки, которым мало одной строки манифеста: предел движка стоит на СЧЁТЧИКАХ пресета,
// противоречие видно только между строками, а формы прикрепляются к осям в конце пресета.
// Грамматика строки живёт в `preset_parse.cpp` и о них ничего не знает — зовёт по имени.
namespace framework::input {
namespace {

std::string row_name(const BindingRow& r) {
    ::input::Source s;
    s.kind = static_cast<::input::SourceKind>(r.kind);
    s.code = static_cast<uint16_t>(r.code);
    s.sign = static_cast<int8_t>(r.sign);
    return source_name(s);
}

bool row_bound(const BindingRow& r) {
    return r.kind != static_cast<uint32_t>(::input::SourceKind::None);
}

// Логических осей в пресете меньше, чем строк: одна ось объявляется несколькими (клавиши, стрелки,
// стик), а предел движка стоит на ОСЯХ. `PresetTable::bind` считает так же — иначе пекарь отбивал бы
// не то, что не влезает.
bool declares_axis(const PresetBuild& b, const PresetRow& p, const std::string& name) {
    for (uint32_t i = p.axis_begin; i < p.axis_begin + p.axis_count; ++i)
        if (b.axis_names[i] == name) return true;
    return false;
}

uint32_t logical_axes(const PresetBuild& b, const PresetRow& p) {
    uint32_t n = 0;
    for (uint32_t i = p.axis_begin; i < p.axis_begin + p.axis_count; ++i) {
        bool first = true;
        for (uint32_t k = p.axis_begin; k < i; ++k)
            if (b.axis_names[k] == b.axis_names[i]) { first = false; break; }
        if (first) ++n;
    }
    return n;
}

// Один источник, тянущий одну ось в обе стороны, — противоречие манифеста, а не альтернатива:
// `ActionMap::resolve` оставляет ПОСЛЕДНИЙ активный источник, поэтому исход решает порядок строк.
// Направление считается ролью (плюс/минус) вместе со знаком самого источника: `padaxis:-ly` в
// положительной роли тянет ось вниз, и по одной роли судить нельзя.
bool axis_pulls_both_ways(const PresetBuild& b, const PresetRow& p, const std::string& name,
                          std::string& source) {
    struct Pull { uint32_t kind, code; int dir; };
    std::vector<Pull> seen;
    for (uint32_t i = p.axis_begin; i < p.axis_begin + p.axis_count; ++i) {
        if (b.axis_names[i] != name) continue;
        const BindingRow* rows[2] = {&b.axes[i].pos, &b.axes[i].neg};
        for (int role = 0; role < 2; ++role) {
            const BindingRow& r = *rows[role];
            if (!row_bound(r)) continue;
            const int dir = (role == 0 ? 1 : -1) * r.sign;
            for (const Pull& s : seen)
                if (s.kind == r.kind && s.code == r.code && s.dir != dir) {
                    source = row_name(r);
                    return true;
                }
            seen.push_back(Pull{r.kind, r.code, dir});
        }
    }
    return false;
}

} // namespace

// Ёмкость движка спрашивается ЗДЕСЬ, а не при загрузке: `PresetTable::bind` отвергает пресет
// целиком и молча, возвращая false, а у пекаря есть и номер строки, и оба числа.
bool preset_check_action_row(const PresetBuild& b, int line, PresetBakeError& err) {
    if (b.presets.back().action_count <= ::input::MAX_ACTIONS) return true;
    return preset_fail(err, line, "the preset declares more than " +
                           std::to_string(::input::MAX_ACTIONS) + " actions; the engine holds "
                           "them in a 64-bit set and binds no more");
}

bool preset_check_axis_row(const PresetBuild& b, const std::string& name, int line,
                           PresetBakeError& err) {
    std::string clash;
    if (axis_pulls_both_ways(b, b.presets.back(), name, clash))
        return preset_fail(err, line, "axis '" + name + "' takes '" + clash + "' in both directions; "
                               "the later row would silently win and reverse the axis");
    if (logical_axes(b, b.presets.back()) > static_cast<uint32_t>(::input::MAX_AXES))
        return preset_fail(err, line, "the preset declares more than " +
                               std::to_string(::input::MAX_AXES) + " axes; the engine binds no more");
    return true;
}

bool preset_close(PresetBuild& b, PresetBakeError& err, int line) {
    if (b.presets.empty()) return true;
    PresetRow& p = b.presets.back();
    // Форма, чью ось пресет не объявляет, до этой проверки исчезала МОЛЧА, а ось оставалась с
    // умолчаниями `parse_axis` — то есть с НУЛЕВОЙ мёртвой зоной: опечатка в имени давала стик,
    // пропускающий шум покоя целиком. Формы потребляются обходом объявленных осей ниже, поэтому
    // неприкреплённую там не встречает никто — её и не встречали.
    // Названа первая ПО СТРОКЕ манифеста, а не первая по алфавиту: `shapes` — упорядоченный map,
    // и без этого диагностика указывала бы на произвольную из нескольких.
    const PresetShape* lost = nullptr;
    const std::string* lost_name = nullptr;
    for (const auto& entry : b.shapes) {
        if (declares_axis(b, p, entry.first)) continue;
        if (lost == nullptr || entry.second.line < lost->line) {
            lost = &entry.second;
            lost_name = &entry.first;
        }
    }
    if (lost != nullptr)
        return preset_fail(err, lost->line, "shape '" + *lost_name + "' names an axis which the "
                               "preset does not declare; its response curve would be dropped and the "
                               "axis would keep a zero deadzone");
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

} // namespace framework::input
