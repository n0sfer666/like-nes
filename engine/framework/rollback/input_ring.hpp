#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>

#include "plan.hpp"

// Кольцо ввода: что каждый игрок нажал на каждом тике окна, и чем считался тик, ввод на который
// ещё не приехал.
//
// Шаблон по типу ввода, а не конкретный `InputFrame`, потому что слой отката не вправе знать, из
// чего состоит ввод игры: спека #4 держит его POD'ом ради записи и реплея, и ровно эта форма нужна
// здесь — побайтовое сравнение и копия без аллокаций.
//
// СЛОТ ПОМЕЧЕН НОМЕРОМ ТИКА, А НЕ ОДНИМ ФЛАГОМ. Кольцо переиспользует слот каждые `capacity`
// тиков, и флаг «подтверждён», переживший круг, — это чужая правда, выданная за здешнюю: тик,
// которому никто не присылал ввод, считался бы подтверждённым вводом тика на круг назад, а
// приехавшая правда объявлялась бы РАСХОЖДЕНИЕМ ИСТОЧНИКОВ. Измерено, а не предположено: без
// штампа гейт сессии печатал ненулевой счётчик конфликтов на прогоне, где двух источников нет
// вовсе.
//
// ПРЕДСКАЗАНИЕ ИЩЕТСЯ ПО ИСТОРИИ, А НЕ ХРАНИТСЯ ПОЛЕМ. Соблазн держать «последний ввод игрока»
// отдельным полем есть, и он неверен: откат переигрывает тики, история за спиной при этом меняется,
// и поле, обновлённое по ходу первого прогона, отдало бы при переигрывании ввод из БУДУЩЕГО
// относительно переигрываемого тика. Обход назад по кольцу такого состояния не имеет вовсе.
namespace framework::rollback {

template <class Input>
class InputRing {
    static_assert(std::is_trivially_copyable<Input>::value,
                  "input of a rollback session is copied by value into the ring");
    // Сравнение — ПОЛЯМИ, и это требование к типу, а не удобство. Байтовое сравнение здесь стояло
    // до 2026-09-03 и лгало: у `character::MoveInput` (fix32 + три bool) есть байт выравнивания, в
    // него никто не пишет, и два ввода с одинаковыми полями расходились в нём. Следствие видно
    // только на настоящем вводе — `game_platformer_rollback_test` дал 420 откатов на 420 тиках и
    // ненулевой счётчик конфликтов, потому что КАЖДОЕ подтверждение читалось как изменение.
    // Игрушечный ввод гейта рядом набивки не имеет и молчал бы дальше.
    static_assert(std::is_same<decltype(std::declval<const Input&>() == std::declval<const Input&>()),
                               bool>::value,
                  "input of a rollback session is compared field by field, never bytewise");

public:
    // Кольцо выписывается один раз: аллокация в тике запрещена инвариантом 5 фреймворка, а тик
    // отката — такой же тик.
    void reset(uint32_t players, uint32_t capacity) {
        players_ = players;
        capacity_ = capacity;
        slots_.assign(static_cast<size_t>(players) * capacity, Input{});
        confirmed_.assign(slots_.size(), 0);
        stamp_.assign(slots_.size(), 0);
    }

    uint32_t players() const { return players_; }
    uint32_t capacity() const { return capacity_; }

    // Ввод тика подряд по игрокам — это и есть форма, в которой его получает симуляция: указатель
    // на `players()` элементов.
    const Input* row(Tick t) const { return &slots_[slot(t, 0)]; }
    const Input& at(Tick t, uint32_t p) const { return slots_[slot(t, p)]; }

    bool confirmed(Tick t, uint32_t p) const {
        const size_t i = slot(t, p);
        return confirmed_[i] != 0 && stamp_[i] == t;
    }

    bool differs(Tick t, uint32_t p, const Input& in) const {
        return !(slots_[slot(t, p)] == in);
    }

    void write(Tick t, uint32_t p, const Input& in) {
        const size_t i = slot(t, p);
        slots_[i] = in;
        confirmed_[i] = 1;
        stamp_[i] = t;
    }

    // Недостающий ввод — повтор последнего подтверждённого этого игрока (политика предсказания
    // спеки #22). Подтверждённые слоты не трогаются: предсказание не вправе затереть правду.
    //
    // Зовётся ПЕРЕД каждым шагом, в том числе при переигрывании: тик, чей ввод так и не приехал,
    // обязан быть пересчитан заново — история перед ним могла измениться тем самым откатом.
    void predict(Tick t) {
        for (uint32_t p = 0; p < players_; ++p) {
            const size_t i = slot(t, p);
            if (confirmed_[i] != 0 && stamp_[i] == t) continue;
            slots_[i] = last_known(t, p);
            confirmed_[i] = 0;
            stamp_[i] = t;
        }
    }

private:
    size_t slot(Tick t, uint32_t p) const {
        return static_cast<size_t>(t % capacity_) * players_ + p;
    }

    Input last_known(Tick t, uint32_t p) const {
        for (uint32_t d = 1; d <= t && d < capacity_; ++d) {
            const size_t i = slot(t - d, p);
            if (confirmed_[i] != 0 && stamp_[i] == t - d) return slots_[i];
        }
        // Истории нет вовсе — только в начале сессии. Умолчание типа, а не «последний ввод
        // соседа»: покой предсказывается покоем.
        return Input{};
    }

    std::vector<Input> slots_;
    std::vector<uint8_t> confirmed_;
    std::vector<Tick> stamp_;
    uint32_t players_ = 0;
    uint32_t capacity_ = 0;
};

} // namespace framework::rollback
