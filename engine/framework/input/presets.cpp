#include "presets.hpp"

#include <cstring>

namespace framework::input {
namespace {

::input::Source to_source(const BindingRow& r) {
    ::input::Source s;
    s.kind = static_cast<::input::SourceKind>(r.kind);
    s.code = static_cast<uint16_t>(r.code);
    s.sign = static_cast<int8_t>(r.sign);
    return s;
}

template <typename T>
const T* view(const uint8_t* base, uint32_t offset, uint32_t count, std::size_t size) {
    // Проверка «влезает ли секция» — единственная защита zero-parse формата: дальше по нему
    // ходят указателями, и битый бандл иначе читался бы как чужая память.
    if (static_cast<std::size_t>(offset) + static_cast<std::size_t>(count) * sizeof(T) > size)
        return nullptr;
    return reinterpret_cast<const T*>(base + offset);
}

} // namespace

bool PresetTable::open(const void* data, std::size_t size) {
    header_ = nullptr;
    if (data == nullptr || size < sizeof(PresetHeader)) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    const auto* h = reinterpret_cast<const PresetHeader*>(base);
    if (std::memcmp(h->magic, PRESET_MAGIC, sizeof(h->magic)) != 0) return false;
    if (h->version != PRESET_VERSION || h->total_size > size) return false;

    presets_ = view<PresetRow>(base, h->presets_offset, h->preset_count, size);
    actions_ = view<ActionRow>(base, h->actions_offset, h->action_count, size);
    axes_ = view<AxisRow>(base, h->axes_offset, h->axis_count, size);
    bindings_ = view<BindingRow>(base, h->bindings_offset, h->binding_count, size);
    pads_ = view<PadRow>(base, h->pads_offset, h->pad_count, size);
    if (presets_ == nullptr || actions_ == nullptr || axes_ == nullptr || bindings_ == nullptr ||
        pads_ == nullptr)
        return false;
    if (h->strings_offset >= h->total_size) return false;

    strings_ = reinterpret_cast<const char*>(base + h->strings_offset);
    strings_size_ = h->total_size - h->strings_offset;
    if (strings_[strings_size_ - 1] != '\0') return false;   // обход имён обязан упереться в ноль
    header_ = h;
    return true;
}

const char* PresetTable::string_at(uint32_t offset) const {
    if (header_ == nullptr || offset >= strings_size_) return "";
    return strings_ + offset;
}

const PresetRow* PresetTable::preset_at(uint32_t index) const {
    if (header_ == nullptr || index >= header_->preset_count) return nullptr;
    return presets_ + index;
}

int PresetTable::find_preset(const char* name) const {
    if (header_ == nullptr || name == nullptr) return -1;
    for (uint32_t i = 0; i < header_->preset_count; ++i)
        if (std::strcmp(string_at(presets_[i].name_offset), name) == 0) return static_cast<int>(i);
    return -1;
}

const char* PresetTable::preset_name(uint32_t preset) const {
    const PresetRow* p = preset_at(preset);
    return p != nullptr ? string_at(p->name_offset) : "";
}

uint32_t PresetTable::action_count(uint32_t preset) const {
    const PresetRow* p = preset_at(preset);
    return p != nullptr ? p->action_count : 0;
}

// Одна ось живёт в нескольких строках (клавиши, стрелки, стик — альтернативные биндинги того же
// движения), поэтому наружу торчит ЛОГИЧЕСКИЙ номер: порядковый номер имени, а не строки.
// Иначе игра, добавив стик в манифест, получила бы сдвиг осей в InputFrame и другой sim-хеш.
uint32_t PresetTable::axis_count(uint32_t preset) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < p->axis_count; ++i)
        if (row_is_first(*p, i)) ++n;
    return n;
}

bool PresetTable::row_is_first(const PresetRow& p, uint32_t row) const {
    const char* name = string_at(axes_[p.axis_begin + row].name_offset);
    for (uint32_t k = 0; k < row; ++k)
        if (std::strcmp(string_at(axes_[p.axis_begin + k].name_offset), name) == 0) return false;
    return true;
}

uint32_t PresetTable::logical_axis(const PresetRow& p, uint32_t row) const {
    uint32_t logical = 0;
    for (uint32_t i = 0; i < row; ++i)
        if (row_is_first(p, i)) ++logical;
    return logical;
}

int PresetTable::first_row_of_axis(const PresetRow& p, uint32_t axis) const {
    for (uint32_t i = 0; i < p.axis_count; ++i)
        if (row_is_first(p, i) && logical_axis(p, i) == axis) return static_cast<int>(i);
    return -1;
}

int PresetTable::find_action(uint32_t preset, const char* name) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr || name == nullptr) return -1;
    for (uint32_t i = 0; i < p->action_count; ++i)
        if (std::strcmp(string_at(actions_[p->action_begin + i].name_offset), name) == 0)
            return static_cast<int>(i);
    return -1;
}

const char* PresetTable::action_name(uint32_t preset, uint32_t action) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr || action >= p->action_count) return "";
    return string_at(actions_[p->action_begin + action].name_offset);
}

uint32_t PresetTable::action_binding_count(uint32_t preset, uint32_t action) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr || action >= p->action_count) return 0;
    return actions_[p->action_begin + action].binding_count;
}

bool PresetTable::action_source(uint32_t preset, uint32_t action, uint32_t which,
                                ::input::Source& out) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr || action >= p->action_count) return false;
    const ActionRow& a = actions_[p->action_begin + action];
    if (which >= a.binding_count) return false;
    out = to_source(bindings_[a.binding_begin + which]);
    return true;
}

int PresetTable::find_axis(uint32_t preset, const char* name) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr || name == nullptr) return -1;
    for (uint32_t i = 0; i < p->axis_count; ++i)
        if (std::strcmp(string_at(axes_[p->axis_begin + i].name_offset), name) == 0)
            return static_cast<int>(logical_axis(*p, i));
    return -1;
}

StickShape PresetTable::axis_shape(uint32_t preset, uint32_t axis) const {
    StickShape s;
    const PresetRow* p = preset_at(preset);
    if (p == nullptr) return s;
    const int row = first_row_of_axis(*p, axis);
    if (row < 0) return s;
    const AxisRow& a = axes_[p->axis_begin + static_cast<uint32_t>(row)];
    s.deadzone = fix32::from_raw(a.deadzone_raw);
    s.outer = fix32::from_raw(a.outer_raw);
    s.curve_exp = a.curve_exp;
    return s;
}

uint32_t PresetTable::axis_pair(uint32_t preset, uint32_t axis) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr) return NO_PAIR;
    const int row = first_row_of_axis(*p, axis);
    if (row < 0) return NO_PAIR;
    return axes_[p->axis_begin + static_cast<uint32_t>(row)].pair_axis;
}

namespace {

bool contains_ci(const char* hay, const char* needle) {
    if (hay == nullptr || needle == nullptr || *needle == '\0') return false;
    for (const char* h = hay; *h != '\0'; ++h) {
        const char* a = h;
        const char* b = needle;
        while (*a != '\0' && *b != '\0') {
            const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
            const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b - 'A' + 'a') : *b;
            if (ca != cb) break;
            ++a;
            ++b;
        }
        if (*b == '\0') return true;
    }
    return false;
}

} // namespace

PadProfile PresetTable::profile_for(const ::input::PadInfo& info) const {
    PadProfile p;
    if (header_ == nullptr) return p;
    const PadRow* hit = nullptr;
    if (info.vid != 0)
        for (uint32_t i = 0; i < header_->pad_count && hit == nullptr; ++i)
            if (pads_[i].vid == info.vid && (pads_[i].pid == 0 || pads_[i].pid == info.pid))
                hit = pads_ + i;
    // Имя — не запасной путь «на всякий случай», а единственный канал на двух ОС из трёх.
    for (uint32_t i = 0; i < header_->pad_count && hit == nullptr; ++i)
        if (pads_[i].match_offset != 0 && contains_ci(info.name, string_at(pads_[i].match_offset)))
            hit = pads_ + i;
    if (hit == nullptr) return p;

    p.name = string_at(hit->name_offset);
    p.labels = static_cast<PadLabels>(hit->labels <= 2 ? hit->labels : 0u);
    p.stick.deadzone = fix32::from_raw(hit->deadzone_raw);
    p.stick.outer = fix32::from_raw(hit->outer_raw);
    p.stick.curve_exp = hit->curve_exp;
    p.trigger_threshold = fix32::from_raw(hit->trigger_raw);
    return p;
}

bool PresetTable::bind(uint32_t preset, ::input::ActionMap& map, int context) const {
    const PresetRow* p = preset_at(preset);
    if (p == nullptr) return false;
    if (p->action_count > ::input::MAX_ACTIONS || axis_count(preset) > ::input::MAX_AXES)
        return false;

    for (uint32_t i = 0; i < p->action_count; ++i) {
        const ActionRow& a = actions_[p->action_begin + i];
        for (uint32_t b = 0; b < a.binding_count; ++b)
            map.bind(static_cast<int>(i), to_source(bindings_[a.binding_begin + b]), context);
    }
    // Мёртвая зона уходит в ActionMap как есть: радиальную по паре осей считает уже слой
    // фреймворка (`radial`), потому что ActionMap знает про оси поодиночке.
    for (uint32_t i = 0; i < p->axis_count; ++i) {
        const AxisRow& a = axes_[p->axis_begin + i];
        map.bind_axis(static_cast<int>(logical_axis(*p, i)), to_source(a.pos), to_source(a.neg),
                      fix32::from_raw(a.deadzone_raw), context);
    }
    return true;
}

} // namespace framework::input
