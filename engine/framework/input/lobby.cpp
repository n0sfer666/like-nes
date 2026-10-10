#include "lobby.hpp"

#include "layout_keys.hpp"

namespace framework::input {

Lobby::Lobby(::input::ActionMap& map, int seats)
    : map_(map), seats_(seats < 0 ? 0 : seats > ::input::MAX_PLAYERS ? ::input::MAX_PLAYERS : seats) {}

bool Lobby::join(int player, ::input::PlayerAssign a, ::input::SharedInput* shared) {
    if (!in_range(player) || state_[player] != SeatState::Free) return false;
    if (a.pad_slot < -1 || a.pad_slot >= ::input::MAX_DEVICES) return false;
    if (a.pad_slot >= 0 && pad_taken(a.pad_slot)) return false;
    if (!map_.assign_player(player, a, shared)) return false;
    state_[player] = SeatState::Present;
    return true;
}

void Lobby::leave(int player) {
    if (!in_range(player) || state_[player] == SeatState::Free) return;
    if (map_.assign_player(player, ::input::PlayerAssign{})) state_[player] = SeatState::Free;
}

// Первое состояние только засевает прошлое: кнопка, зажатая ещё в меню до уровня, — не нажатие входа.
// promote раньше mark_lost: пад, выдернутый на тике сразу после входа, ставит паузу в этом же тике.
void Lobby::update(const ::input::DeviceState& d) {
    if (!seeded_) {
        prev_ = d;
        seeded_ = true;
    }
    promote();
    mark_lost(d);
    for (int slot = 0; slot < ::input::MAX_DEVICES; ++slot) {
        const bool edge = (d.pad_btns[slot] & ~prev_.pad_btns[slot]) != 0;
        if (edge && d.pad_connected[slot] && !pad_taken(slot)) claim_pad(slot);
    }
    claim_keys(d);
    prev_ = d;
}

SeatState Lobby::state(int player) const { return in_range(player) ? state_[player] : SeatState::Free; }

bool Lobby::present(int player) const {
    const SeatState s = state(player);
    return s == SeatState::Present || s == SeatState::Lost || s == SeatState::Resuming;
}

bool Lobby::lost(int player) const { return state(player) == SeatState::Lost; }

bool Lobby::paused() const { return first(SeatState::Lost) >= 0 || first(SeatState::Resuming) >= 0; }

int Lobby::first(SeatState s) const {
    for (int p = 0; p < seats_; ++p)
        if (state_[p] == s) return p;
    return -1;
}

bool Lobby::pad_taken(int slot) const {
    for (int p = 0; p < seats_; ++p)
        if (state_[p] != SeatState::Free && map_.assignment(p).pad_slot == slot) return true;
    return false;
}

void Lobby::promote() {
    for (int p = 0; p < seats_; ++p)
        if (state_[p] == SeatState::Joining || state_[p] == SeatState::Resuming) state_[p] = SeatState::Present;
}

// Потерявший пад отпускает и сам слот: ОС вернёт пад под младшим свободным индексом, не обязательно
// прежним, а слот, оставшийся за ним в карте, отказал бы следующему владельцу как общий вход.
void Lobby::mark_lost(const ::input::DeviceState& d) {
    for (int p = 0; p < seats_; ++p) {
        const ::input::PlayerAssign a = map_.assignment(p);
        if (state_[p] != SeatState::Present || a.pad_slot < 0) continue;
        if (a.pad_slot < ::input::MAX_DEVICES && d.pad_connected[a.pad_slot]) continue;
        if (map_.assign_player(p, ::input::PlayerAssign{-1, a.use_kbd_mouse})) state_[p] = SeatState::Lost;
    }
}

void Lobby::claim_pad(int slot) {
    const int lost = first(SeatState::Lost);
    if (lost >= 0) {
        const ::input::PlayerAssign a{slot, map_.assignment(lost).use_kbd_mouse};
        if (map_.assign_player(lost, a)) state_[lost] = SeatState::Resuming;
        return;
    }
    const int free = first(SeatState::Free);
    if (free >= 0 && map_.assign_player(free, ::input::PlayerAssign{slot, false})) state_[free] = SeatState::Joining;
}

void Lobby::claim_keys(const ::input::DeviceState& d) {
    for (int p = 0; p < seats_; ++p) {
        if (state_[p] != SeatState::Free || !layout_key_pressed(map_.layout(p), d, prev_)) continue;
        if (map_.assign_player(p, ::input::PlayerAssign{-1, true})) state_[p] = SeatState::Joining;
    }
}

} // namespace framework::input
