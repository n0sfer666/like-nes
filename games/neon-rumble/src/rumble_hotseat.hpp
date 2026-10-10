#pragma once
#include <array>
#include <cstdint>

#include "action_map.hpp"
#include "input_engine.hpp"
#include "lobby.hpp"
#include "rumble_command.hpp"
#include "seat.hpp"

namespace rumble {

using Players = std::array<PlayerCommand, framework::brawl::SEATS>;

// Двое за устройствами: P1 сидит на клавиатуре с открытия, P2 входит кнопкой пада или клавишей
// своей половины клавиатуры. Переходы мест печатаются строкой `seat tick N: Pk <состояние>` — по
// ним §29 судит вход, паузу и выход на живом железе.
class Hotseat {
public:
    bool open();
    input::InputEngine& engine() { return engine_; }
    const framework::input::Lobby& lobby() const { return lobby_; }

    // Возвращает false в паузе: драка в этот тик не шагает.
    bool tick(uint32_t t, Players& out);

private:
    void report(uint32_t t);

    input::ActionMap map_;
    input::InputEngine engine_{map_};
    framework::input::Lobby lobby_{map_, static_cast<int>(framework::brawl::SEATS)};
    std::array<framework::input::SeatState, framework::brawl::SEATS> was_{};
};

} // namespace rumble
