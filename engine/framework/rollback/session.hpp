#pragma once
#include <cstdint>
#include <vector>

#include "input_ring.hpp"
#include "plan.hpp"

// Сессия отката: кольцо снимков, кольцо ввода и план переигрывания над ЛЮБОЙ симуляцией.
//
// Шаблон, а не интерфейс с виртуальными методами: симуляция обязана оставаться той же, что в
// одиночной игре, — решение 1 спеки #22 гонит по сети ввод, а не мир, и это верно ровно до тех
// пор, пока откатный прогон и обычный идут ОДНИМ кодом. Требования к параметру:
//
//     using Input    = <POD ввода одного игрока>;
//     using Snapshot = <состояние симуляции>;
//     void save(Snapshot&) const;
//     void restore(const Snapshot&);
//     void step(const Input* players);   // подряд по игрокам, ровно `players()` штук
//
// Сеть сюда не входит ни строкой: `deliver` — это «ввод стал известен», а откуда он приехал,
// сессию не касается. Поэтому шаг A вертикали проверяется целиком без сокетов.
namespace framework::rollback {

template <class Sim>
class Session {
public:
    using Input = typename Sim::Input;
    using Snapshot = typename Sim::Snapshot;

    // Насколько вперёд разрешено принимать ввод. Задержка ввода (input delay) — вторая половина
    // откатной модели: она не сетевая величина, а игровая, и приезжает вводом на будущий тик.
    static constexpr uint32_t LEAD = 16;

    // Кольца выписываются здесь и больше не растут. Снимков `depth + 1`, потому что откат на
    // `depth` тиков назад означает возврат в состояние ПЕРЕД тем тиком.
    void reset(uint32_t players, uint32_t depth) {
        plan_.reset(depth);
        ring_.reset(players, depth + LEAD + 2);
        snaps_.assign(depth + 1, Snapshot{});
        rollbacks_ = 0;
        replayed_ = 0;
        conflicts_ = 0;
        too_far_ = 0;
    }

    Tick tick() const { return plan_.head(); }

    // Ввод стал известен. `false` — он не принят, и это ЗАЯВЛЕННЫЙ исход: слишком поздно (снимок
    // вытеснен) или слишком далеко вперёд (места в кольце нет). Оба посчитаны отдельно, потому что
    // лечатся разным: первое — глубиной, второе — задержкой ввода.
    bool deliver(Tick t, uint32_t player, const Input& in) {
        if (t > plan_.head() + LEAD) {
            ++too_far_;
            return false;
        }
        const bool known = ring_.confirmed(t, player);
        const bool changed = ring_.differs(t, player, in);
        // Два разных подтверждённых значения на один тик — это не «поздний ввод», а расхождение
        // источников: детерминизм отсюда уже не следует, и молчать об этом нельзя.
        if (known && changed) ++conflicts_;
        ring_.write(t, player, in);
        // Совпал с предсказанием — откатываться не за чем. Ради этого случая предсказание и
        // существует: верная догадка стоит ноль переигранных тиков.
        if (!changed) return true;
        return plan_.note_dirty(t);
    }

    // Один тик сессии: сначала долги прошлого, потом текущий тик.
    void advance(Sim& sim) {
        resolve(sim);
        step_one(sim);
    }

    // Догнать долги, не делая нового тика. Нужно там, где прогон сравнивают с эталоном на
    // конкретном номере тика: сравнивать состояние, в котором ещё висит непогашенный откат,
    // значит сравнивать предсказание с правдой.
    void settle(Sim& sim) { resolve(sim); }

    uint32_t rollbacks() const { return rollbacks_; }
    uint32_t replayed() const { return replayed_; }
    uint32_t conflicts() const { return conflicts_; }
    uint32_t too_deep() const { return plan_.too_deep(); }
    uint32_t too_far() const { return too_far_; }

private:
    void resolve(Sim& sim) {
        if (!plan_.pending()) return;
        const uint32_t count = plan_.replay_count();
        sim.restore(snaps_[plan_.from() % snaps_.size()]);
        plan_.rewind();
        // Переигрывание идёт ТЕМ ЖЕ `step_one`, что и первый прогон. Не экономия строк:
        // переигранный тик обязан быть неотличим от исходного, а отдельная ветка переигрывания —
        // это второй код тика, который однажды разойдётся с первым.
        for (uint32_t i = 0; i < count; ++i) step_one(sim);
        ++rollbacks_;
        replayed_ += count;
    }

    void step_one(Sim& sim) {
        const Tick t = plan_.head();
        ring_.predict(t);
        // Снимок снимается ПЕРЕД шагом: точка возврата — состояние, из которого тик ещё можно
        // сыграть заново. Снимок после шага вернул бы уже испорченный неверным вводом мир.
        sim.save(snaps_[t % snaps_.size()]);
        sim.step(ring_.row(t));
        plan_.advanced();
    }

    ReplayPlan plan_;
    InputRing<Input> ring_;
    std::vector<Snapshot> snaps_;
    uint32_t rollbacks_ = 0;
    uint32_t replayed_ = 0;
    uint32_t conflicts_ = 0;
    uint32_t too_far_ = 0;
};

} // namespace framework::rollback
