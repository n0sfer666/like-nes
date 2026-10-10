#include "action_map.hpp"

#include <utility>

namespace input {
namespace {

// Индекс «пола» контекстного стека: верхний consume-контекст блокирует нижние.
int consume_floor(const std::vector<Context>& stack) {
    int floor = 0;
    for (int i = 0; i < static_cast<int>(stack.size()); ++i)
        if (stack[i].consume) floor = i;
    return floor;
}

bool source_pressed(const Source& s, const DeviceState& d, const PlayerAssign& pa) {
    switch (s.kind) {
    case SourceKind::Key:         return pa.use_kbd_mouse && d.key_down(s.code);
    case SourceKind::MouseButton: return pa.use_kbd_mouse && d.mouse_down(s.code);
    case SourceKind::PadButton:   return pa.pad_slot >= 0 && d.pad_down(pa.pad_slot, s.code);
    default: return false;
    }
}

fix32 source_axis(const Source& s, const DeviceState& d, const PlayerAssign& pa) {
    switch (s.kind) {
    case SourceKind::PadAxis:
        if (pa.pad_slot < 0) return fix32{};
        return d.pad_axis(pa.pad_slot, s.code) * fix32::from_int(s.sign);
    case SourceKind::MouseAxis:
        if (!pa.use_kbd_mouse) return fix32{};
        return (s.code == 0 ? d.frame_dx : s.code == 1 ? d.frame_dy : d.frame_wheel) * fix32::from_int(s.sign);
    case SourceKind::Key: // клавиша-как-ось: полное отклонение
        return (pa.use_kbd_mouse && d.key_down(s.code)) ? fix32::from_int(s.sign) : fix32{};
    default: return fix32{};
    }
}

// Линейная мёртвая зона по модулю (целочисл. fix32, детерм.): |v|<=dz → 0; иначе
// нормализуем остаток на (1-dz), сохраняя знак и диапазон [-1,1].
fix32 apply_deadzone(fix32 v, fix32 dz) {
    if (dz.raw < 0) dz = fix32{};                                  // кламп dz в [0, 1)
    if (dz.raw >= fix32::ONE) dz = fix32::from_raw(fix32::ONE - 1); // denom > 0 гарантирован
    fix32 mag = v.raw < 0 ? -v : v;
    if (!(dz < mag)) return fix32{};          // mag <= dz
    fix32 one = fix32::from_int(1);
    fix32 denom = one - dz;
    fix32 scaled = (mag - dz) / denom;
    if (one < scaled) scaled = one;           // clamp
    return v.raw < 0 ? -scaled : scaled;
}

} // namespace

bool ActionMap::context_active(int ctx) const {
    if (stack_.empty()) return ctx == 0; // без контекстов активен дефолтный 0
    int floor = consume_floor(stack_);
    for (int i = floor; i < static_cast<int>(stack_.size()); ++i)
        if (stack_[i].id == ctx) return true;
    return false;
}

bool ActionMap::shared_with_others(int player, const ActionLayout& l, const PlayerAssign& pa,
                                   SharedInput* shared) const {
    for (int other = 0; other < MAX_PLAYERS; ++other) {
        Source src;
        if (other == player || !find_shared_input(l, pa, layouts_[other], players_[other], src)) continue;
        if (shared != nullptr) {
            const bool pad = src.kind == SourceKind::PadButton || src.kind == SourceKind::PadAxis;
            *shared = {player, other, src, pad ? pa.pad_slot : -1};
        }
        return true;
    }
    return false;
}

bool ActionMap::set_layout(int player, ActionLayout layout, SharedInput* shared) {
    if (player < 0 || player >= MAX_PLAYERS) return false;
    if (shared_with_others(player, layout, players_[player], shared)) return false;
    layouts_[player] = std::move(layout);
    return true;
}

bool ActionMap::assign_player(int player, PlayerAssign a, SharedInput* shared) {
    if (player < 0 || player >= MAX_PLAYERS) return false;
    if (shared_with_others(player, layouts_[player], a, shared)) return false;
    players_[player] = a;
    return true;
}

InputFrame ActionMap::resolve(const DeviceState& d, int player, uint32_t tick, uint64_t prev_held) const {
    InputFrame f;
    f.tick = tick;
    if (player < 0 || player >= MAX_PLAYERS) return f;
    const PlayerAssign& pa = players_[player];
    const ActionLayout& layout = layouts_[player];

    for (const ActionBinding& b : layout.buttons()) {
        if (!context_active(b.context)) continue;
        if (source_pressed(b.src, d, pa)) f.held |= (1ull << b.action); // OR-семантика
    }
    for (const AxisBinding& b : layout.axes()) {
        if (!context_active(b.context)) continue;
        if (b.axis < 0 || b.axis >= MAX_AXES) continue;
        fix32 v;
        if (b.neg.kind != SourceKind::None) v = source_axis(b.pos, d, pa) - source_axis(b.neg, d, pa);
        else v = source_axis(b.pos, d, pa);
        v = v * b.scale;                        // чувствительность (напр. масштаб дельты мыши)
        v = apply_deadzone(v, b.deadzone);
        if (!(v == fix32{})) f.axes[b.axis] = v; // последний активный источник выигрывает
    }

    f.pressed = f.held & ~prev_held;
    f.released = ~f.held & prev_held;
    return f;
}

} // namespace input
