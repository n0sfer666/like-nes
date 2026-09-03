#pragma once
#include <cstdint>

#include "stream.hpp"

// Верификация реплея (спека #22, гейт 6): переигрываем присланный поток ввода СВОЕЙ симуляцией и
// сверяем заявленный хеш каждого тика со своим.
//
// Требование к симуляции — то же, что у `Session`, плюс одно:
//
//     using Input = <POD ввода одного игрока>;
//     void step(const Input* players);   // подряд по игрокам, ровно `players` штук
//     uint64_t hash() const;             // состояние ПОСЛЕ шага, одним числом
//
// Переигрывание идёт БЕЗ отката и без предсказания: у верификатора весь ввод известен заранее, и
// откат тут был бы вторым кодом тика — тем самым, который однажды разойдётся с первым. Тик у него
// тот же `step`, что у живого прогона, поэтому «поток принят» означает «этот прогон воспроизводим
// НАШИМ движком», а не «числа сошлись».
namespace framework::replay {

// Исход — не `bool`, потому что отказов у него три и лечатся они разным: разошёлся ввод (подделка
// или другая сборка), поток пуст (проверено НИЧЕГО — принять такое значит выдать зелёный вердикт о
// пустоте), ширина строки не та (поток от другого числа игроков, и его строки поедут в `step`
// сдвинутыми).
enum class Reason : uint8_t { Match, Diverged, Empty, Players };

struct Verdict {
    Reason reason = Reason::Empty;
    // При `Diverged` — номер ПЕРВОГО разошедшегося тика. При `Match` — сколько тиков проверено:
    // «принят» без этого числа неотличим от «принят, потому что смотреть было нечего».
    Tick tick = 0;

    bool ok() const { return reason == Reason::Match; }
};

inline const char* reason_name(Reason r) {
    switch (r) {
        case Reason::Match: return "match";
        case Reason::Diverged: return "diverged";
        case Reason::Empty: return "empty stream";
        case Reason::Players: return "player count";
    }
    return "unknown";
}

template <class Sim>
Verdict verify(Sim& sim, const Stream<typename Sim::Input>& s, uint32_t players) {
    if (s.players() != players) return Verdict{Reason::Players, 0};
    if (s.ticks() == 0) return Verdict{Reason::Empty, 0};
    for (Tick t = 0; t < s.ticks(); ++t) {
        sim.step(s.row(t));
        // Сверка НА КАЖДОМ тике, а не в конце: расхождение, найденное на тике `t`, дальше только
        // размазывается — состояние уже не то, и все последующие хеши не совпадут по следствию.
        // Первый несовпавший и есть ответ гейта.
        if (sim.hash() != s.claim(t)) return Verdict{Reason::Diverged, t};
    }
    return Verdict{Reason::Match, s.ticks()};
}

} // namespace framework::replay
