#pragma once
#include <cstdint>
#include <vector>
#include "device_state.hpp"
#include "input_types.hpp"

// Слой действий: device-agnostic биндинги → InputFrame. Контексты (стек + consume),
// dead-zone (radial 2D / linear 1D, целочисл. fix32), несколько биндингов на действие (OR),
// per-player device assignment. Карта — ДАННЫЕ (перебиндивая в рантайме, детерм.).
namespace input {

struct Source {
    SourceKind kind = SourceKind::None;
    uint16_t code = 0;  // key/button/axis-код
    int8_t sign = 1;    // направление для оси-из-клавиш / инверсия pad-оси
};

struct ActionBinding {
    int action = 0;
    Source src;
    int context = 0;
};

// Ось: пара источников (pos/neg — клавиши-как-ось) ИЛИ одиночная pad/mouse-ось.
struct AxisBinding {
    int axis = 0;
    Source pos;         // + направление (или сама ось для pad/mouse)
    Source neg;         // - направление (kind==None если одиночная ось)
    fix32 deadzone;     // радиус мёртвой зоны (fix32)
    int context = 0;
    fix32 scale = fix32::from_int(1); // чувствительность (напр. масштаб дельты мыши)
};

struct Context { int id = 0; bool consume = false; };

// listen-next: сырое button-down событие → Source для рантайм-перебиндивания (UI — спека #7).
inline Source capture_source(const RawEvent& e) {
    switch (e.kind) {
    case RawKind::KeyDown:         return {SourceKind::Key, e.code, 1};
    case RawKind::MouseButtonDown: return {SourceKind::MouseButton, e.code, 1};
    case RawKind::PadButtonDown:   return {SourceKind::PadButton, e.code, 1};
    default:                       return {};
    }
}

// Назначение устройств игрокам: pad-слот игрока; kbd/mouse → игрок kbd_player.
struct PlayerAssign {
    int pad_slot = -1;         // -1 = нет геймпада
    bool use_kbd_mouse = false;
};

class ActionMap {
public:
    void bind(int action, Source src, int context = 0) { buttons_.push_back({action, src, context}); }
    void bind_axis(int axis, Source pos, Source neg, fix32 dz, int context = 0, fix32 scale = fix32::from_int(1)) {
        axes_.push_back({axis, pos, neg, dz, context, scale});
    }

    void push_context(int id, bool consume) { stack_.push_back({id, consume}); }
    void pop_context() { if (!stack_.empty()) stack_.pop_back(); }

    // Rebind (карта = данные): заменить which-й биндинг действия / очистить действие.
    void rebind(int action, int which, Source s) {
        int seen = 0;
        for (ActionBinding& b : buttons_)
            if (b.action == action && seen++ == which) { b.src = s; return; }
    }
    void clear_action(int action) {
        for (auto it = buttons_.begin(); it != buttons_.end();)
            it->action == action ? it = buttons_.erase(it) : ++it;
    }

    void assign_player(int player, PlayerAssign a) { if (player >= 0 && player < MAX_PLAYERS) players_[player] = a; }

    // Разрешить состояние устройств → InputFrame игрока за тик.
    // prev_held — уровень Action прошлого тика (для edge pressed/released).
    InputFrame resolve(const DeviceState& d, int player, uint32_t tick, uint64_t prev_held) const;

private:
    bool context_active(int ctx) const;
    bool source_pressed(const Source& s, const DeviceState& d, const PlayerAssign& pa) const;
    fix32 source_axis(const Source& s, const DeviceState& d, const PlayerAssign& pa) const;

    std::vector<ActionBinding> buttons_;
    std::vector<AxisBinding> axes_;
    std::vector<Context> stack_;
    PlayerAssign players_[MAX_PLAYERS];
};

} // namespace input
