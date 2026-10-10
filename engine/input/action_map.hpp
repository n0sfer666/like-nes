#pragma once
#include <cstdint>
#include <vector>
#include "action_layout.hpp"
#include "action_shared.hpp"
#include "device_state.hpp"
#include "input_types.hpp"

// Слой действий: device-agnostic биндинги → InputFrame. Контексты (стек + consume),
// dead-zone (radial 2D / linear 1D, целочисл. fix32), несколько биндингов на действие (OR),
// per-player device assignment. Карта — ДАННЫЕ (перебиндивая в рантайме, детерм.).
namespace input {

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

class ActionMap {
public:
    // Общий физический вход у двух игроков — отказ, а не тихое удвоение: одна клавиша жала бы
    // кнопку обоим. Карта при отказе не меняется; игрок вне [0, MAX_PLAYERS) отвергается всегда.
    [[nodiscard]] bool set_layout(int player, ActionLayout layout, SharedInput* shared = nullptr);
    [[nodiscard]] bool assign_player(int player, PlayerAssign a, SharedInput* shared = nullptr);

    // Игрок вне диапазона читает пустые раскладку и назначение.
    const ActionLayout& layout(int player) const;
    PlayerAssign assignment(int player) const;

    void push_context(int id, bool consume) { stack_.push_back({id, consume}); }
    void pop_context() { if (!stack_.empty()) stack_.pop_back(); }

    // Разрешить состояние устройств → InputFrame игрока за тик. Игрок вне диапазона получает
    // пустой кадр, а не раскладку игрока 0. prev_held — уровень Action прошлого тика.
    InputFrame resolve(const DeviceState& d, int player, uint32_t tick, uint64_t prev_held) const;

private:
    bool context_active(int ctx) const;
    bool shared_with_others(int player, const ActionLayout& l, const PlayerAssign& pa, SharedInput* shared) const;

    ActionLayout layouts_[MAX_PLAYERS];
    PlayerAssign players_[MAX_PLAYERS];
    std::vector<Context> stack_;
};

} // namespace input
