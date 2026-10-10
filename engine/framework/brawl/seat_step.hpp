#pragma once
#include <span>

#include "body_pool.hpp"
#include "brawl_input.hpp"
#include "brawl_step.hpp"
#include "seat.hpp"

namespace framework::brawl {

// Первая система тика — тело появляется раньше ударов, и ввод тика входа уже доходит до него.
// Input{} — отсутствие: так InputRing отвечает до первого ввода игрока. Место, чьё тело снято не
// здесь (KO в игре), получает новое: присутствующий игрок без тела слал бы ввод в никуда.
void step_seats(Seats& seats, BodyPool& pool, std::span<const BrawlInput, SEATS> inputs,
                std::span<const Body, SEATS> spawns);

// Остальным телам — пустая команда: буфер переиспользуется между тиками, и прошлый ввод не должен
// дойти до манекена. Удар выбирает игра.
void seat_commands(const Seats& seats, const BodyPool& pool, std::span<const BrawlInput, SEATS> inputs,
                   std::span<Command> out);

} // namespace framework::brawl
