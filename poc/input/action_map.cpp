#include "action_map.hpp"

namespace input {

// Индекс «пола» контекстного стека: верхний consume-контекст блокирует нижние.
static int consume_floor(const std::vector<Context>& stack) {
    int floor = 0;
    for (int i = 0; i < static_cast<int>(stack.size()); ++i)
        if (stack[i].consume) floor = i;
    return floor;
}

bool ActionMap::context_active(int ctx) const {
    if (stack_.empty()) return ctx == 0; // без контекстов активен дефолтный 0
    int floor = consume_floor(stack_);
    for (int i = floor; i < static_cast<int>(stack_.size()); ++i)
        if (stack_[i].id == ctx) return true;
    return false;
}

bool ActionMap::source_pressed(const Source& s, const DeviceState& d, const PlayerAssign& pa) const {
    switch (s.kind) {
    case SourceKind::Key:         return pa.use_kbd_mouse && d.key_down(s.code);
    case SourceKind::MouseButton: return pa.use_kbd_mouse && d.mouse_down(s.code);
    case SourceKind::PadButton:   return pa.pad_slot >= 0 && d.pad_down(pa.pad_slot, s.code);
    default: return false;
    }
}

fix32 ActionMap::source_axis(const Source& s, const DeviceState& d, const PlayerAssign& pa) const {
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
static fix32 apply_deadzone(fix32 v, fix32 dz) {
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

InputFrame ActionMap::resolve(const DeviceState& d, int player, uint32_t tick, uint64_t prev_held) const {
    InputFrame f;
    f.tick = tick;
    const PlayerAssign& pa = (player >= 0 && player < MAX_PLAYERS) ? players_[player] : players_[0];

    for (const ActionBinding& b : buttons_) {
        if (!context_active(b.context)) continue;
        if (source_pressed(b.src, d, pa)) f.held |= (1ull << b.action); // OR-семантика
    }
    for (const AxisBinding& b : axes_) {
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
