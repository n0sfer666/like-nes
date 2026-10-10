#pragma once
#include <cstdint>
#include <span>

#include "ai_state.hpp"
#include "body_pool.hpp"
#include "brawl_step.hpp"

namespace framework::ai {

// Всё, что функция состояния вправе читать и менять. Мира вне симуляции здесь нет намеренно, а мир драки
// есть: удар мозг берёт из архетипа своего тела, и другого пути к таблице бойца у функции игры нет.
struct Mind {
    Brain& brain;
    const brawl::Body& body;
    const brawl::BodyPool& pool;
    const brawl::BrawlWorld& world;
    AttackTokens& tokens;
    WorldRng& rng;
    uint32_t tick;
};

using Think = void (*)(Mind& mind, brawl::Command& out);

// Сначала снимаются ВСЕ мозги, чьи тела пропали из пула, с их жетонами — иначе мозг с меньшим seq
// получил бы отказ от жетона, который держит уже мёртвое тело. Затем по выжившим: цель без тела
// сбрасывается, таймер убывает до нуля ДО вызова состояния. Команды — по индексу тела в пуле, как
// у seat_commands, и к вызову уже обнулены: буфер вне снапшота, и непереписанный слот унёс бы в тик
// команду отброшенной после отката ветки.
void step_ai(AiState& ai, const brawl::BodyPool& pool, const brawl::BrawlWorld& world, uint32_t tick,
             std::span<const Think> states, std::span<brawl::Command> commands);

} // namespace framework::ai
