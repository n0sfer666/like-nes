#pragma once
#include "action_map.hpp"
#include "device_state.hpp"
#include "input_types.hpp"
#include "seat_state.hpp"

// Хотсит: кто из игроков за каким устройством. Лобби живёт вне симуляции — устройство локально,
// в сети #27 пад чужого пира не значит ничего; в симуляцию уходит только present во вводе тика.
//
// update зовётся после InputEngine::drain и до resolve: занятое в тике устройство разрешается в
// том же тике, и нажатие входа становится уровнем, а не фронтом удара на тике появления.
namespace framework::input {

class Lobby {
public:
    Lobby(::input::ActionMap& map, int seats);

    // Вход без нажатия — игрок, сидящий с первого тика. Отказ карты (общий вход) — отказ входа, и
    // занятый пад отказывает, даже если раскладки не делят кнопку: один unplug потерял бы обоих.
    [[nodiscard]] bool join(int player, ::input::PlayerAssign a, ::input::SharedInput* shared = nullptr);
    void leave(int player);

    // Фронт кнопки свободного пада: сначала возвращает потерявшего пад, иначе сажает на младшее
    // свободное место. Фронт клавиши из раскладки свободного игрока сажает именно его.
    void update(const ::input::DeviceState& d);

    int seats() const { return seats_; }
    SeatState state(int player) const;
    bool present(int player) const;
    bool lost(int player) const;
    bool paused() const;

private:
    bool in_range(int player) const { return player >= 0 && player < seats_; }
    int first(SeatState s) const;
    bool pad_taken(int slot) const;
    void promote();
    void mark_lost(const ::input::DeviceState& d);
    void claim_pad(int slot);
    void claim_keys(const ::input::DeviceState& d);

    ::input::ActionMap& map_;
    int seats_;
    SeatState state_[::input::MAX_PLAYERS] = {};
    ::input::DeviceState prev_;
    bool seeded_ = false;
};

} // namespace framework::input
